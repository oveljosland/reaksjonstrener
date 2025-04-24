#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h> // https://github.com/vshymanskyy/Preferences

#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

/*--- game ---*/
#define _DEFAULT 9999
#define TOO_SLOW 9998
#define WRONGBTN 9997

#define TIMEOUT_MS 500
#define INIT_LIVES 5
volatile int8_t lives = INIT_LIVES;
unsigned long start_ms = 0;
short score = 0;

/*--- oled ---*/
#define DISP_W 128
#define DISP_H 64
#define RESET -1
Adafruit_SSD1306 oled(DISP_W, DISP_H, &Wire, RESET);

/*--- pins ---*/
#define N_PINS 4
const uint8_t led_pins[] = {25, 33, 32, 12};
const uint8_t btn_pins[] = {13, 14, 27, 26};

/*--- leds ---*/
#define TRIG_DEF 50
#define TRIG_MIN 0
#define TRIG_MAX 2500UL
volatile uint8_t active_led = RESET;
unsigned long led_on_ms = 0;
uint8_t wait = 0;

/* mutex for shared state */
SemaphoreHandle_t mtx;

/* task handles */
TaskHandle_t leds_handle = NULL;
TaskHandle_t btns_handle = NULL;
TaskHandle_t oled_handle = NULL;

QueueHandle_t rtq; /* queue to send reaction time to display */
QueueHandle_t bpq; /* queue for button presses */

/* for saving high score to non-volatile memory */
Preferences prefs;

void IRAM_ATTR btn_isr(void *params)
{
  int btn_idx = (int) params;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(bpq, &btn_idx, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken)
    portYIELD_FROM_ISR();
}

void setup(void)
{
  Serial.begin(115200);
  randomSeed(analogRead(34));

  for (uint8_t i = 0; i < N_PINS; i++) {
    pinMode(led_pins[i], OUTPUT);
    pinMode(btn_pins[i], INPUT_PULLUP);
    attachInterruptArg(btn_pins[i], btn_isr, (void*) i, FALLING);
  }

  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    Serial.println("oled: allocation failed");

  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(10, 20);
  oled.print("react!");
  oled.display();

  prefs.begin("react", false);

  /* optionally reset best reaction time */
  if (digitalRead(btn_pins[0]) == LOW) {
    prefs.putULong("best_rt", _DEFAULT);
    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.setTextSize(2);
    oled.print("reset");
    oled.display();
    Serial.println("game: high score reset");

    while (digitalRead(btn_pins[0]) == LOW)
      delay(10);
  }

  /* initialize mutex and queue */
  mtx = xSemaphoreCreateMutex();
  rtq = xQueueCreate(1, sizeof(unsigned long));
  bpq = xQueueCreate(10, sizeof(short)); /* holds button indxes */

  /* create tasks */
  xTaskCreatePinnedToCore(task_leds,"leds",2048,NULL,1,&leds_handle, app_cpu);
  xTaskCreatePinnedToCore(task_btns,"btns",2048,NULL,2,&btns_handle, app_cpu);
  xTaskCreatePinnedToCore(task_oled,"oled",2048,NULL,1,&oled_handle, app_cpu);
}

void loop(void)
{
  /* empty */
}

void task_leds(void *params)
{
  while (1) {
    if (!wait && lives > 0) {
      if (score == 0 && start_ms == 0)
        start_ms = millis();
      char led = random(0, N_PINS);
      xSemaphoreTake(mtx, portMAX_DELAY);
      active_led = led;
      led_on_ms = millis();
      wait = 1;
      digitalWrite(led_pins[led], HIGH);
      xSemaphoreGive(mtx);
    }

    if (wait) {
      if (millis() - led_on_ms > TIMEOUT_MS) {
        xSemaphoreTake(mtx, portMAX_DELAY);
        digitalWrite(led_pins[active_led], LOW);
        active_led = RESET;
        wait = 0;
        lives--;
        unsigned long rt = TOO_SLOW;
        xQueueSend(rtq, &rt, portMAX_DELAY);
        xSemaphoreGive(mtx);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(TRIG_DEF + random(TRIG_MIN, TRIG_MAX)));
  }
}

void task_btns(void *params)
{
  int8_t idx;

  while (1) {
    if (xQueueReceive(bpq, &idx, portMAX_DELAY)) {
      xSemaphoreTake(mtx, portMAX_DELAY);
      if (wait) {
        if (idx == active_led) {
          unsigned long rt = millis() - led_on_ms;
          digitalWrite(led_pins[active_led], LOW);
          active_led = RESET;
          wait = 0;
          score++;
          xQueueSend(rtq, &rt, portMAX_DELAY);
        } else {
          lives--;
          digitalWrite(led_pins[active_led], LOW);
          active_led = RESET;
          wait = 0;
          unsigned long rt = WRONGBTN;
          xQueueSend(rtq, &rt, portMAX_DELAY);
        }
      }
      xSemaphoreGive(mtx);
    }
  }
}

void task_oled(void *params)
{
  unsigned long rt;
  unsigned long best_rt = prefs.getULong("best_rt", _DEFAULT);

  while (1) {
    if (xQueueReceive(rtq, &rt, portMAX_DELAY)) {
      uint8_t new_hs = 0;
      if (rt < best_rt) {
        best_rt = rt;
        prefs.putULong("best_rt", best_rt);
        new_hs = 1;
      }

      oled.clearDisplay();
      oled.setCursor(0, 0);
      oled.setTextSize(1);
      oled.print("lives: ");
      oled.print(lives);
      oled.setCursor(0, 16);
      oled.setTextSize(2);

      if (rt == WRONGBTN)
        oled.print("wrong btn!");
      else if (rt == TOO_SLOW) 
        oled.print("too slow!");
      else {
        oled.print(rt);
        oled.print(" ms");
      }

      oled.setTextSize(1);
      oled.setCursor(0, 48);
      if (new_hs)
        oled.print("new high score!");
      else {
        oled.print("best: ");
        oled.print(best_rt);
        oled.print(" ms");
      }

      if (lives <= 0) {
        unsigned long survived_ms = millis() - start_ms;
        oled.clearDisplay();
        oled.setCursor(0, 0);
        oled.setTextSize(1);
        oled.print("you lost");

        oled.setCursor(0, 16);
        oled.print("survived: ");
        oled.print(survived_ms / 1000.0, 2);
        oled.print(" s");

        oled.setCursor(0, 32);
        oled.print("score: ");
        oled.print(score);

        oled.display();
        vTaskDelay(pdMS_TO_TICKS(5000));

        lives = INIT_LIVES;
        score = 0;
        start_ms = 0;
        
        oled.clearDisplay();
        oled.setCursor(10, 20);
        oled.setTextSize(2);
        oled.print("react!");
        oled.display();
      }
      oled.display();    
    }
  }
}

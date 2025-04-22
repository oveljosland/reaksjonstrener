#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdlib.h>

#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

#define N_PINS 4
#define DISP_W 128
#define DISP_H 64
#define RESET -1
Adafruit_SSD1306 oled(DISP_W, DISP_H, &Wire, RESET);

const uint8_t led_pins[] = {25, 33, 32, 12};
const uint8_t btn_pins[] = {13, 14, 27, 26};

#define TRIG_MIN 500
#define TRIG_MAX 1500
volatile uint8_t led = RESET;
unsigned long led_on_ms = 0;
uint8_t wait = 0;

/* mutex for shared state */
SemaphoreHandle_t mut;

/* task handles */
TaskHandle_t leds_handle = NULL;
TaskHandle_t btns_handle = NULL;
TaskHandle_t oled_handle = NULL;

/* queue to send reaction time to display */
QueueHandle_t rtq;

void setup(void)
{
  Serial.begin(115200);
  randomSeed(analogRead(0));

  /* initialize pins */
  for (uint8_t i = 0; i < N_PINS; i++) {
    pinMode(led_pins[i], OUTPUT);
    pinMode(btn_pins[i], INPUT_PULLUP);
  }

  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    Serial.println("oled: allocation failed");

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.print("reaction trainer");
  oled.display();

  /* initialize mutex and queue */
  mut = xSemaphoreCreateMutex();
  rtq = xQueueCreate(1, sizeof(unsigned long));

  /* create tasks */
  xTaskCreatePinnedToCore(task_leds, "leds", 2048, NULL, 1, &leds_handle, app_cpu);
  xTaskCreatePinnedToCore(task_btns, "btns", 2048, NULL, 2, &btns_handle, app_cpu);
  xTaskCreatePinnedToCore(task_oled, "oled", 2048, NULL, 1, &oled_handle, app_cpu);
}

void loop(void)
{
  /* empty */
}

void task_leds(void *params)
{
  while (1) {
    if (!wait) {
      uint8_t r = random(0, N_PINS);
      xSemaphoreTake(mut, portMAX_DELAY);
      led = r;
      led_on_ms = millis();
      wait = 1;
      digitalWrite(led_pins[r], HIGH);
      xSemaphoreGive(mut); 
    }
    vTaskDelay(pdMS_TO_TICKS(500 + random(TRIG_MIN, TRIG_MAX)));
  }
}

void task_btns(void *params)
{
  while (1) {
    if (wait) {
      for (uint8_t i = 0; i < N_PINS; i++) {
        if (digitalRead(btn_pins[i]) == LOW) {
          xSemaphoreTake(mut, portMAX_DELAY);
          if (i == led) {
            unsigned long t = millis() - led_on_ms;
            digitalWrite(led_pins[led], LOW);
            led = RESET;
            wait = 0;
            xQueueSend(rtq, &t, portMAX_DELAY);
          }
          xSemaphoreGive(mut);
          while (digitalRead(btn_pins[i]) == LOW)
            vTaskDelay(pdMS_TO_TICKS(10));
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void task_oled(void *params)
{
  unsigned long t;
  while (1) {
    if (xQueueReceive(rtq, &t, portMAX_DELAY)) {
      oled.clearDisplay();
      oled.setCursor(0, 0);
      oled.print("reaction time:");
      oled.setCursor(0, 20);
      oled.setTextSize(2);
      oled.print(t);
      oled.print(" ms");
      oled.setTextSize(1);
      oled.display();
    }
  }
}

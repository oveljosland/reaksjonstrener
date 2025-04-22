// Bruker kun kjerne 1 for demonstrasjon
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

#define DISP_W 128 /* display width */
#define DISP_H 64 /* display height */
#define RST -1 /* reset */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define N_PINS 4

#define L1 25
#define L2 33
#define L3 32
#define L4 12

#define B1 13
#define B2 14
#define B2 27
#define B2 25


/* check if leds.state[i] == btns.state[i] */
Adafruit_SSD1306 display(DISP_W, DISP_H, &Wire, RST);

struct port {
  uint8_t pins[N_PINS];
  uint8_t states[N_PINS];
};

struct port btns = {{B1, B2, B3, B4}, {0}};
struct port leds = {{L1, L2, L3, L4}, {0}};

/* ensures ID is saved in the interactor task, identifies which button is pressed */
static SemaphoreHandle_t bin_sem;

/* identifier for interactor task */
static TaskHandle_t interactor_task;

/* ISR for button presses. awakens the interactor task thread */
void IRAM_ATTR ISR_BTN() {
  BaseType_t task_woken = pdFALSE;

  vTaskNotifyGiveFromISR(interactor_task, &task_woken);

  if (task_woken) {
    portYIELD_FROM_ISR();
  }
}

/*
* Interactor-oppgaven, som reagerer pÃ¥ knappetrykk og styrer en LED basert pÃ¥ dette. Laget for Ã¥ kunne brukes for
* flere knappe-LED-par ved Ã¥ ha en parameteriserbar ID.  En binÃ¦r semafor brukes for Ã¥ signalisere at IDen er lagret lokalt.
*/
void interactor(void *parameters) {
  int interactor_id = *(int *)parameters;

  for (uint8_t i = 0; i < N_PINS; i++)
    leds.states[i] = 0;

  Serial.print("task ready:");
  Serial.println(interactor_id);
  
  xSemaphoreGive(bin_sem);
  while(1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    for (uint8_t i = 0; i < N_PINS; i++) {
      digitalWrite(leds.pins[i],leds.states[i]);
      leds.states[i] = !leds.states[i];
    }
  }
}

void setup()
{
  Serial.begin(115200);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    Serial.println("display: allocation failed");

  display.display();
  delay(100);
  display.clearDisplay();

  Serial.println("--- reaction trainer ---");

  char task_name[14];
  uint8_t interactor_id = 0;

  bin_sem = xSemaphoreCreateBinary();

  for (uint8_t i = 0; i < N_PINS; i++) {
    pinMode(btns.pins[i], INPUT_PULLUP);
    pinMode(leds.pins[i], OUTPUT);
  }

  for (uint8_t i = 0; i < N_PINS; i++)
    attachInterrupt(btns.pins[i], ISR_BTN, FALLING); /* attach interrupts to buttons*/

  sprintf(task_name, "interactor %i", interactor_id);
  xTaskCreatePinnedToCore(
    interactor,
    task_name,
    1024,
    (void *)&interactor_id,
    1,
    &interactor_task,
    app_cpu
    );
  xSemaphoreTake(bin_sem, portMAX_DELAY);
}

void loop()
{
  /* empty */
}
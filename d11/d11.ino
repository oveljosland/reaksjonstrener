// Bruker kun kjerne 1 for demonstrasjon
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

const int btnPin = 12;
const int ledPin = 13;

/* 
* BinÃ¦r semafor for Ã¥ sÃ¸rge for at id-nummeret blir lageret i interactor-tasken. 
* Dette brukes for Ã¥ identifisere hvilken knapp som har blitt trykket inn.
*/
static SemaphoreHandle_t binSem;

/* Identifikator for interactor-oppgaven */
static TaskHandle_t interactorTask;

/*
* Avbruddsrutine for knappetrykk. Vekker den pÃ¥koblete interactorTask-trÃ¥den. 
*/
void IRAM_ATTR ISR_BTN() {
  BaseType_t task_woken = pdFALSE;

  vTaskNotifyGiveFromISR(interactorTask, &task_woken);

  if (task_woken) {
    portYIELD_FROM_ISR();
  }
}

/*
* Interactor-oppgaven, som reagerer pÃ¥ knappetrykk og styrer en LED basert pÃ¥ dette. Laget for Ã¥ kunne brukes for
* flere knappe-LED-par ved Ã¥ ha en parameteriserbar ID.  En binÃ¦r semafor brukes for Ã¥ signalisere at IDen er lagret lokalt.
*/
void interactor(void *parameters) {
  int interactorId = *(int *)parameters;
  int ledState = 0;

  Serial.print("Task ready:");
  Serial.println(interactorId);
  
  xSemaphoreGive(binSem);
  while(1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    digitalWrite(ledPin,ledState);
    ledState = !ledState;
  }
}

void setup() {
    Serial.begin(115200);
    Serial.println("---FreeRTOS btn and led demo---");

    char task_name[14];
    uint8_t interactorId = 0;

    binSem = xSemaphoreCreateBinary();

    pinMode(btnPin, INPUT_PULLUP);
    pinMode(ledPin, OUTPUT);

    attachInterrupt(btnPin, ISR_BTN, FALLING);

    sprintf(task_name, "Interactor %i", interactorId);
    xTaskCreatePinnedToCore(interactor,
                            task_name,
                            1024,
                            (void *)&interactorId,
                            1,
                            &interactorTask,
                            app_cpu);
    xSemaphoreTake(binSem, portMAX_DELAY);
  
}

void loop() {


}
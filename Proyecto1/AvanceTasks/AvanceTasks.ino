#include <Arduino.h>

#define MAX_BARCOS 5
const int buttonPin = 22;

SemaphoreHandle_t buttonSemaphore;

// Semáforo contador: representa los slots disponibles (inicia en MAX_BARCOS)
SemaphoreHandle_t slotsDisponibles;

static int barcoID = 0;

// ---------------- ISR ----------------
void IRAM_ATTR handleButtonInterrupt() {
  static uint32_t lastPressTime = 0;
  uint32_t now = millis();

  // Ignorar si han pasado menos de 300ms desde el último disparo
  if ((now - lastPressTime) < 300) return;
  lastPressTime = now;

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(buttonSemaphore, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// ---------------- TASK BARCO ----------------
void BarcoTask(void *parameter) {
  int id = *((int*) parameter);
  

  Serial.print("[Barco ");
  Serial.print(id);
  Serial.println("] Creado");

  // Simular trabajo del barco
  vTaskDelay(pdMS_TO_TICKS(3000));

  Serial.print("[Barco ");
  Serial.print(id);
  Serial.println("] Terminado");

  // Liberar su slot antes de morir
  xSemaphoreGive(slotsDisponibles);
  vTaskDelete(NULL);
}

// ---------------- TASK CREADOR ----------------
void CreadorTask(void *parameter) {
  for (;;) {
    // Espera señal del botón
    if (xSemaphoreTake(buttonSemaphore, portMAX_DELAY) == pdTRUE) {

      // Debounce dentro del task
      vTaskDelay(pdMS_TO_TICKS(200));

      // Intentar tomar un slot (no bloqueante, para no congelar el creador)
      if (xSemaphoreTake(slotsDisponibles, 0) == pdTRUE) {
        int *param = (int*) malloc(sizeof(int));
        if (param == NULL) {
          Serial.println("[Creador] Error: malloc falló");
          xSemaphoreGive(slotsDisponibles); // devolver slot si no hay memoria
          continue;
        }
        *param = barcoID;

        char nombre[16];
        snprintf(nombre, sizeof(nombre), "Barco_%d", barcoID);

        BaseType_t resultado = xTaskCreate(
          BarcoTask,
          nombre,
          2048,
          param,
          1,
          NULL
        );

        if (resultado != pdPASS) {
          Serial.println("[Creador] Error: xTaskCreate falló");
          free(param);
          xSemaphoreGive(slotsDisponibles); // devolver slot
        } else {
          Serial.print("[Creador] Barco creado ID: ");
          Serial.println(barcoID);
          barcoID++;
        }

      } else {
        Serial.println("[Creador] Límite de barcos alcanzado");
      }
    }
  }
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);

  buttonSemaphore   = xSemaphoreCreateBinary();
  slotsDisponibles  = xSemaphoreCreateCounting(MAX_BARCOS, MAX_BARCOS);

  attachInterrupt(digitalPinToInterrupt(buttonPin), handleButtonInterrupt, FALLING);

  xTaskCreate(
    CreadorTask,
    "CreadorTask",
    2048,    // stack un poco más grande por snprintf
    NULL,
    2,
    NULL
  );
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
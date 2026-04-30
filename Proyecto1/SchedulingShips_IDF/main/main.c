#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include "ship.h"

#define MAX_BARCOS 5

#define BTN_NORMAL   4
#define BTN_PESQUERO 5
#define BTN_PATRULLA 6

static const char *TAG = "MAIN";

SemaphoreHandle_t buttonSemaphore;
QueueHandle_t buttonQueue;
SemaphoreHandle_t slotsDisponibles;

static int barcoID = 0;

// ---------------- ISR ----------------
void IRAM_ATTR gpio_isr_handler(void* arg) {

    static uint32_t lastPressTime = 0;

    uint32_t now = xTaskGetTickCountFromISR();

    // 300 ms en ticks
    if ((now - lastPressTime) < pdMS_TO_TICKS(300)) {
        return;
    }

    lastPressTime = now;

    int pin = (int) arg;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xQueueSendFromISR(buttonQueue, &pin, &xHigherPriorityTaskWoken);
    xSemaphoreGiveFromISR(buttonSemaphore, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// ---------------- TASK BARCO ----------------
void BarcoTask(void *parameter) {
    Ship* barco = (Ship*) parameter;

    ESP_LOGI(TAG, "Barco creado ID: %d Tipo: %s",
    barco->id,
    shipTypeToString(barco->type));

    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Barco terminado ID: %d", barco->id);

    free(barco);
    xSemaphoreGive(slotsDisponibles);

    vTaskDelete(NULL);
}

// ---------------- TASK CREADOR ----------------
void CreadorTask(void *parameter) {
    int pin;

    for (;;) {
        if (xSemaphoreTake(buttonSemaphore, portMAX_DELAY)) {

            // leer qué botón fue
            if (xQueueReceive(buttonQueue, &pin, 0)) {

                if (xSemaphoreTake(slotsDisponibles, 0)) {

                    Ship* barco = malloc(sizeof(Ship));
                    if (!barco) {
                        ESP_LOGE(TAG, "malloc falló");
                        xSemaphoreGive(slotsDisponibles);
                        continue;
                    }

                    ShipType tipo;

                    if (pin == BTN_NORMAL) tipo = NORMAL;
                    else if (pin == BTN_PESQUERO) tipo = PESQUERO;
                    else tipo = PATRULLA;

                    inicializar_barco(barco, barcoID++, tipo);

                    xTaskCreate(
                        BarcoTask,
                        "BarcoTask",
                        2048,
                        barco,
                        1,
                        NULL
                    );

                } else {
                    ESP_LOGW(TAG, "Límite de barcos alcanzado");
                }
            }
        }
    }
}

// ---------------- INIT GPIO ----------------
void init_buttons() {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask =
            (1ULL << BTN_NORMAL) |
            (1ULL << BTN_PESQUERO) |
            (1ULL << BTN_PATRULLA),
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };

    gpio_config(&io_conf);

    gpio_install_isr_service(0);

    gpio_isr_handler_add(BTN_NORMAL, gpio_isr_handler, (void*) BTN_NORMAL);
    gpio_isr_handler_add(BTN_PESQUERO, gpio_isr_handler, (void*) BTN_PESQUERO);
    gpio_isr_handler_add(BTN_PATRULLA, gpio_isr_handler, (void*) BTN_PATRULLA);
}

// ---------------- MAIN ----------------
void app_main(void) {

    buttonSemaphore = xSemaphoreCreateBinary();
    buttonQueue = xQueueCreate(10, sizeof(int));
    slotsDisponibles = xSemaphoreCreateCounting(MAX_BARCOS, MAX_BARCOS);

    init_buttons();

    xTaskCreate(
        CreadorTask,
        "CreadorTask",
        4096,
        NULL,
        2,
        NULL
    );
}
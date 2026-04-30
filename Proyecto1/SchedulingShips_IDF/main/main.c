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
#include "ready_queue.h"

/*
 * Pines GPIO usados para los botones.
 *
 * Cada botón crea un barco de un tipo diferente.
 */
#define BTN_NORMAL   4
#define BTN_PESQUERO 5
#define BTN_PATRULLA 6

/*
 * Pin GPIO usado como DIP switch para seleccionar la dirección.
 *
 * 0 -> IZQUIERDA
 * 1 -> DERECHA
 */
#define BTN_COLA     22

static const char *TAG = "MAIN";

/*
 * Semáforo binario usado para avisar a CreadorTask que ocurrió
 * una interrupción de botón.
 */
SemaphoreHandle_t buttonSemaphore;

/*
 * Cola de FreeRTOS donde la ISR guarda el número de pin presionado.
 */
QueueHandle_t buttonQueue;

/*
 * Identificador incremental para asignar un ID único a cada barco.
 */
static int barcoID = 0;

// ---------------- ISR ----------------
/*
 * Manejador de interrupción GPIO.
 *
 * Esta función se ejecuta cuando se detecta un flanco positivo
 * en alguno de los botones configurados.
 *
 * Responsabilidades:
 * - Aplicar un antirrebote básico de 300 ms.
 * - Identificar qué botón fue presionado.
 * - Enviar el pin presionado a una cola de FreeRTOS.
 * - Liberar un semáforo para despertar a CreadorTask.
 *
 * Al ejecutarse dentro de una ISR, se usan funciones especiales
 * terminadas en FromISR.
 */
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

/*
 * Colas principales de barcos.
 *
 * colaIzq: barcos que se dirigen hacia la izquierda.
 * colaDer: barcos que se dirigen hacia la derecha.
 */
QueueShip colaIzq;
QueueShip colaDer;
// ---------------- TASK CREADOR ----------------
void CreadorTask(void *parameter) {
    int pin;

    for (;;) {
        if (xSemaphoreTake(buttonSemaphore, portMAX_DELAY)) {

            if (xQueueReceive(buttonQueue, &pin, 0)) {

                Ship* barco = malloc(sizeof(Ship));
                if (!barco) continue;

                ShipType tipo;
                if (pin == BTN_NORMAL) tipo = NORMAL;
                else if (pin == BTN_PESQUERO) tipo = PESQUERO;
                else tipo = PATRULLA;

                // Lee el DIP switch para seleccionar dirección.
                int estado = gpio_get_level(BTN_COLA);
                Direction dir = estado ? DERECHA : IZQUIERDA;

                inicializar_barco(barco, barcoID++, tipo, dir);

                QueueShip* target = (dir == IZQUIERDA) ? &colaIzq : &colaDer;

                if (enqueue(target, barco)) {
                    ESP_LOGI(TAG, "Barco %d (%s) -> %s",
                        barco->id,
                        shipTypeToString(barco->type),
                        dirToString(dir));

                    printQueue(&colaIzq);
                    printQueue(&colaDer);
                } else {
                    /*
                     * Si no se pudo encolar, se libera la memoria del barco
                     * para evitar fugas de memoria.
                     */
                    free(barco);
                }
            }
        }
    }
}

/*
 * Tarea encargada de simular el paso de barcos por el canal.
 *
 * La tarea intenta sacar un barco primero de la cola izquierda.
 * Si no hay barcos en esa cola, intenta sacar uno de la cola derecha.
 *
 * Cuando encuentra un barco:
 * - Muestra un mensaje en el log.
 * - Espera 3 segundos simulando el cruce.
 * - Libera la memoria del barco.
 */

void CanalTask(void *param) {
    Ship* barco;

    for (;;) {
        if (dequeue(&colaIzq, &barco) || dequeue(&colaDer, &barco)) {

            ESP_LOGI(TAG, "Canal: pasando barco %d (%s)",
                barco->id,
                shipTypeToString(barco->type));

            vTaskDelay(pdMS_TO_TICKS(3000));

            free(barco);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


// ---------------- INIT GPIO ----------------
/*
 * Configura los pines GPIO usados por los botones y el DIP switch.
 *
 * Botones:
 * - Configurados como entrada.
 * - Generan interrupción en flanco positivo.
 *
 * DIP switch:
 * - Configurado como entrada.
 * - No genera interrupción.
 * - Usa pull-down interno.
 */
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

    // Configurar DIP SWITCH
    gpio_config_t dip_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BTN_COLA),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
    };

    gpio_config(&dip_conf);

    gpio_config(&io_conf);

    gpio_install_isr_service(0);

    gpio_isr_handler_add(BTN_NORMAL, gpio_isr_handler, (void*) BTN_NORMAL);
    gpio_isr_handler_add(BTN_PESQUERO, gpio_isr_handler, (void*) BTN_PESQUERO);
    gpio_isr_handler_add(BTN_PATRULLA, gpio_isr_handler, (void*) BTN_PATRULLA);
}

// ---------------- MAIN ----------------
/*
 * Punto de entrada principal en ESP-IDF.
 *
 * Inicializa los recursos compartidos, configura los GPIO,
 * inicializa las colas de barcos y crea las tareas de FreeRTOS.
 */
void app_main(void) {

    buttonSemaphore = xSemaphoreCreateBinary();
    buttonQueue = xQueueCreate(10, sizeof(int));

    init_buttons();

    initQueue(&colaIzq, "COLA IZQUIERDA");
    initQueue(&colaDer, "COLA DERECHA");

    xTaskCreate(CreadorTask, "Creador", 4096, NULL, 2, NULL);
    xTaskCreate(CanalTask, "Canal", 4096, NULL, 1, NULL);
}
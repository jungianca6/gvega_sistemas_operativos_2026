#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "HD44780.h"
#include <driver/i2c.h>
#include "driver/uart.h"

#include "ship.h"
#include "ready_queue.h"
#include "lcd_display.h"
#include "esp_spiffs.h"
#include "config_parser.h"
#include "channel.h"
/*
 * Pines GPIO usados para los botones.
 *
 * Cada botón crea un barco de un tipo diferente.
 */
#define BTN_NORMAL   18
#define BTN_PESQUERO 19
#define BTN_PATRULLA 20

/*
 * Pin GPIO usado como DIP switch para seleccionar la dirección.
 *
 * 0 -> IZQUIERDA
 * 1 -> DERECHA
 */
#define BTN_COLA     22

#define UART_PORT_NUM UART_NUM_0
#define BUF_SIZE 1024

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

/*
 * Configuración global cargada desde config.txt.
 */
static AppConfig appConfig;
static Channel mainChannel;
static int shipSpeeds[3]; // STANDARD, FISHING, PATROL

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

// Forward declarations
void ShipTask(void *parameter);
esp_err_t init_spiffs(void);

void spawn_ship_thread(void *arg) {
    Ship* barco = (Ship*)arg;
    char name[16];
    snprintf(name, sizeof(name), "ShipTask_%d", barco->id);
    xTaskCreate(ShipTask, name, 2048, barco, 1, NULL);
}
// ---------------- TASK CREADOR ----------------
void CreadorTask(void *parameter) {
    // 1. Inicializar SPIFFS y Cargar Configuración
    if (init_spiffs() == ESP_OK) {
        if (parseConfigFile("/spiffs/config.txt", &appConfig) == ESP_OK) {
            shipSpeeds[STANDARD] = appConfig.standard_speed;
            shipSpeeds[FISHING] = appConfig.fishing_speed;
            shipSpeeds[PATROL] = appConfig.patrol_speed;

            init_channel(&mainChannel, 
                         appConfig.channel_length, 
                         appConfig.standard_speed, 
                         appConfig.flow_control, 
                         appConfig.sign_duration, 
                         appConfig.parameter_w);
        }
    }

    // 2. Inicializar Colas
    initQueue(&colaIzq, "COLA IZQUIERDA");
    initQueue(&colaDer, "COLA DERECHA");

    // 3. Poblar barcos iniciales (y crear sus hilos)
    populate_queue_from_config(&colaIzq, appConfig.queue_left, appConfig.queue_left_count, LEFT, shipSpeeds, &barcoID, spawn_ship_thread);
    populate_queue_from_config(&colaDer, appConfig.queue_right, appConfig.queue_right_count, RIGHT, shipSpeeds, &barcoID, spawn_ship_thread);

    int pin;
    for (;;) {
        if (xSemaphoreTake(buttonSemaphore, portMAX_DELAY)) {
            if (xQueueReceive(buttonQueue, &pin, 0)) {
                Ship* barco = malloc(sizeof(Ship));
                if (!barco) continue;

                ShipType tipo;
                if (pin == BTN_NORMAL) tipo = STANDARD;
                else if (pin == BTN_PESQUERO) tipo = FISHING;
                else tipo = PATROL;

                int estado = gpio_get_level(BTN_COLA);
                Direction dir = estado ? RIGHT : LEFT;

                inicializar_barco(barco, barcoID++, tipo, dir, shipSpeeds[tipo]);

                QueueShip* target = (dir == LEFT) ? &colaIzq : &colaDer;

                if (enqueue(target, barco)) {
                    ESP_LOGI(TAG, "Nuevo barco manual %d (%s) -> %s",
                        barco->id, shipTypeToString(barco->type), dirToString(dir));
                    
                    spawn_ship_thread(barco);
                    
                    lcd_mostrar_colas(&colaIzq, &colaDer);
                } else {
                    vSemaphoreDelete(barco->sem);
                    free(barco);
                }
            }
        }
    }
}

void ShipTask(void *parameter) {
    Ship* barco = (Ship*)parameter;

    // El barco espera en su semáforo privado hasta que el planificador le dé paso
    if (xSemaphoreTake(barco->sem, portMAX_DELAY)) {
        // Simulación de cruce (el tiempo real se implementará luego)
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    // El CanalTask (planificador) se encargará de hacer el dequeue y liberar la memoria.
    // Esta tarea simplemente termina.
    vTaskDelete(NULL);
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
        // Por ahora, un planificador simple que da paso al primero que encuentre
        if (dequeue(&colaIzq, &barco) || dequeue(&colaDer, &barco)) {

            ESP_LOGI(TAG, "Planificador: dando paso a barco %d (%s)",
                barco->id, shipTypeToString(barco->type));

            // Despertar el hilo del barco
            xSemaphoreGive(barco->sem);

            // Esperar a que el barco cruce (simulado igual que el ShipTask por ahora)
            vTaskDelay(pdMS_TO_TICKS(3500)); 

            vSemaphoreDelete(barco->sem);
            free(barco);
            lcd_mostrar_colas(&colaIzq, &colaDer);
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

// ---------------- SCROLL TASK ----------------
static LCD_t lcd1, lcd2;

void ScrollTask(void *param) {
    const char *text = "--- ESP32 MULTI-LCD SCROLLING EXAMPLE --- ";
    int len = strlen(text);
    char buffer[17];
    int start = 0;

    for (;;) {
        for (int i = 0; i < 16; i++) {
            buffer[i] = text[(start + i) % len];
        }
        buffer[16] = '\0';

        // Scroll on LCD 1, line 0
        LCD_setCursor(&lcd1, 0, 0);
        LCD_writeStr(&lcd1, buffer);

        // Scroll on LCD 2, line 1
        LCD_setCursor(&lcd2, 0, 1);
        LCD_writeStr(&lcd2, buffer);

        start = (start + 1) % len;
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

// ---------------- UART TASK ----------------
void init_uart() {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
}

void UartReceiverTask(void *pvParameters) {
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    for (;;) {
        int len = uart_read_bytes(UART_PORT_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            data[len] = '\0';
            // Print message as requested
            ESP_LOGI(TAG, "Mensaje recibido Serial: %s", (char *)data);
        }
    }
    free(data);
    vTaskDelete(NULL);
}

esp_err_t init_spiffs(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ESP_OK;
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

    // Initialize both LCDs for scrolling example
    LCD_init(&lcd1, 0x23, 21, 22, 16, 2);
    LCD_init(&lcd2, 0x26, 21, 22, 16, 2);

    // Also initialize the main LCD abstraction for ship tracking
    lcd_init();

    LCD_clearScreen(&lcd1);
    LCD_clearScreen(&lcd2);

    LCD_setCursor(&lcd1, 0, 1);
    LCD_writeStr(&lcd1, "LCD 1 (0x23)");
    LCD_setCursor(&lcd2, 0, 0);
    LCD_writeStr(&lcd2, "LCD 2 (0x26)");

    xTaskCreate(CreadorTask, "Creador", 4096, NULL, 2, NULL);
    xTaskCreate(CanalTask, "Canal", 4096, NULL, 1, NULL);
    //xTaskCreate(ScrollTask, "Scroll", 2048, NULL, 1, NULL);

    init_uart();
    xTaskCreate(UartReceiverTask, "UartReceiver", 4096, NULL, 1, NULL);
}
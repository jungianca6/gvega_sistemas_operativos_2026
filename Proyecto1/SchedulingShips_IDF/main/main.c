#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

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

/* Intervalo base de animación en ms. Se escala con ChannelLength. */
#define BASE_TICK_MS 1000

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

/* Handle de CanalTask para que ShipTask pueda notificar cuando termina */
static TaskHandle_t canal_task_handle = NULL;

/* Semáforo para sincronizar: CanalTask espera hasta que CreadorTask termine el setup inicial */
static SemaphoreHandle_t canal_start_sem;

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
    snprintf(name, sizeof(name), "Ship_%d", barco->id);
    xTaskCreate(ShipTask, name, 2048, barco, 1, &barco->task_handle);
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
            channel_set_global(&mainChannel);
        }
    }

    // 2. Inicializar Colas
    initQueue(&colaIzq, "COLA IZQUIERDA");
    initQueue(&colaDer, "COLA DERECHA");

    // 3. Poblar barcos iniciales (y crear sus hilos)
    populate_queue_from_config(&colaIzq, appConfig.queue_left, appConfig.queue_left_count, LEFT, shipSpeeds, &barcoID, spawn_ship_thread);
    populate_queue_from_config(&colaDer, appConfig.queue_right, appConfig.queue_right_count, RIGHT, shipSpeeds, &barcoID, spawn_ship_thread);

    ESP_LOGI(TAG, "Colas iniciales pobladas. Actualizando LCDs...");

    
    lcd_mostrar_colas(&colaIzq, &colaDer);

    // Señalar a CanalTask que las colas están listas
    xSemaphoreGive(canal_start_sem);

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

    // Bloquear hasta que CanalTask dé permiso de cruzar
    xSemaphoreTake(barco->sem, portMAX_DELAY);

    // Calcular el tick de animación escalado por ChannelLength
    int tick_ms = BASE_TICK_MS;
    if (g_channel) {
        tick_ms = BASE_TICK_MS * g_channel->length / 100;
    }
    if (tick_ms < 50) tick_ms = 50;

    int exit_col = (barco->direction == LEFT) ? 15 : 0;

    ESP_LOGI(TAG, "ShipTask: barco %d iniciando cruce (speed=%d, tick=%dms, exit_col=%d)",
             barco->id, barco->speed, tick_ms, exit_col);

    // Loop de animación
    while (1) {
        // === EMERGENCY STOP CHECK ===
        if (emergency_stop) {
            ESP_LOGI(TAG, "ShipTask: barco %d — PARADA DE EMERGENCIA", barco->id);
            if (canal_task_handle) xTaskNotifyGive(canal_task_handle);
            vTaskDelete(NULL);
            return;
        }

        // ¿Llegamos al final?
        if ((barco->direction == LEFT  && barco->channel_col >= exit_col) ||
            (barco->direction == RIGHT && barco->channel_col <= exit_col)) {
            break;
        }

        // Verificar colisión — SIN busy waiting
        while (!lcd_check_pos(barco)) {
            if (emergency_stop) {
                if (canal_task_handle) xTaskNotifyGive(canal_task_handle);
                vTaskDelete(NULL);
                return;
            }
            xEventGroupWaitBits(channel_event_group,
                                BIT_SHIP_MOVED,
                                pdTRUE, pdFALSE,
                                pdMS_TO_TICKS(500)); // timeout para re-check emergency
        }

        // Avanzar el barco
        lcd_channel_advance_ship(barco);

        // Esperar tick
        vTaskDelay(pdMS_TO_TICKS(tick_ms));
    }

    ESP_LOGI(TAG, "ShipTask: barco %d completó el cruce", barco->id);

    // Cruce normal: limpiar y liberar
    lcd_channel_remove_ship(barco);

    if (canal_task_handle) {
        xTaskNotifyGive(canal_task_handle);
    }

    vSemaphoreDelete(barco->sem);
    free(barco);
    vTaskDelete(NULL);
}

/*
 * Manejo de parada de emergencia.
 * Espera que todas las ShipTasks activas notifiquen su salida,
 * evacúa barcos del canal, los re-encola, muestra candado,
 * y espera hasta que emergency_stop se desactive.
 */
static void handle_emergency_stop(int sent) {
    ESP_LOGW(TAG, "=== PARADA DE EMERGENCIA ACTIVADA ===");

    // Esperar que todos los ShipTasks activos notifiquen su salida
    for (int i = 0; i < sent; i++) {
        ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(2000));
    }

    // Evacuar barcos del canal LCD
    Ship* evac_ships[16];
    int evac_count = lcd_channel_evacuate_all(evac_ships, 16);

    // Re-encolar barcos al frente de sus colas con nuevos semáforos y tareas
    for (int i = 0; i < evac_count; i++) {
        Ship* s = evac_ships[i];
        s->channel_col = -1;
        vSemaphoreDelete(s->sem);
        s->sem = xSemaphoreCreateBinary();

        QueueShip* q = (s->direction == LEFT) ? &colaIzq : &colaDer;
        enqueue_front(q, s);
        spawn_ship_thread(s);

        ESP_LOGI(TAG, "Barco %d (%s) re-encolado en dirección %s",
                 s->id, shipTypeToString(s->type), dirToString(s->direction));
    }

    // Mostrar candado y actualizar colas
    lcd_channel_show_lock();
    lcd_mostrar_colas(&colaIzq, &colaDer);

    // Esperar a que se desactive emergency_stop
    while (emergency_stop) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    lcd_channel_clear_lock();
    ESP_LOGW(TAG, "=== PARADA DE EMERGENCIA FINALIZADA ===");
}

/*
 * Algoritmo Fairness: alterna W barcos de cada dirección.
 * Retorna true si fue interrumpido por emergency_stop.
 */
static bool fairness_dispatch(Direction current_dir) {
    int w = g_channel->w_parameter;
    QueueShip* source = (current_dir == LEFT) ? &colaIzq : &colaDer;
    int row = (current_dir == LEFT) ? 0 : 1;
    int sent = 0;

    ESP_LOGI(TAG, "CanalTask [Fairness]: turno para dirección %s (W=%d)",
             dirToString(current_dir), w);

    // Despachar hasta W barcos
    while (sent < w) {
        if (emergency_stop) {
            handle_emergency_stop(sent);
            return true;
        }

        Ship* barco;
        if (!dequeue(source, &barco)) {
            ESP_LOGI(TAG, "CanalTask: cola %s vacía después de %d barcos",
                     dirToString(current_dir), sent);
            break;
        }

        // Esperar entrada libre — con chequeo de emergencia
        while (!lcd_channel_entry_free(row)) {
            if (emergency_stop) {
                enqueue_front(source, barco);
                handle_emergency_stop(sent);
                return true;
            }
            xEventGroupWaitBits(channel_event_group,
                                BIT_SHIP_MOVED,
                                pdTRUE, pdFALSE,
                                pdMS_TO_TICKS(500));
        }

        if (emergency_stop) {
            enqueue_front(source, barco);
            handle_emergency_stop(sent);
            return true;
        }

        lcd_channel_place_ship(barco, row);
        xSemaphoreGive(barco->sem);
        lcd_mostrar_colas(&colaIzq, &colaDer);

        sent++;
        ESP_LOGI(TAG, "CanalTask: barco %d (%s) despachado (%d/%d)",
                 barco->id, shipTypeToString(barco->type), sent, w);
    }

    // Esperar que todos los barcos crucen
    for (int i = 0; i < sent; i++) {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        if (emergency_stop) {
            handle_emergency_stop(sent - (i + 1));
            return true;
        }
    }

    if (sent > 0) {
        ESP_LOGI(TAG, "CanalTask: %d barcos de dirección %s completaron el cruce",
                 sent, dirToString(current_dir));
    }
    return false;
}

/*
 * Tarea planificadora del canal.
 * Selecciona algoritmo según FlowControl de config.txt.
 */
void CanalTask(void *param) {
    xSemaphoreTake(canal_start_sem, portMAX_DELAY);

    FlowControl mode = (FlowControl)g_channel->flow_control;
    ESP_LOGI(TAG, "CanalTask: modo=%d, W=%d, ChannelLength=%d",
             mode, g_channel->w_parameter, g_channel->length);

    Direction current_dir = LEFT;

    for (;;) {
        // Chequear emergencia antes de cada ciclo
        if (emergency_stop) {
            handle_emergency_stop(0);
            continue;
        }

        switch (mode) {
            case FAIRNESS: {
                bool interrupted = fairness_dispatch(current_dir);
                if (!interrupted) {
                    current_dir = (current_dir == LEFT) ? RIGHT : LEFT;
                }
                break;
            }
            case SIGN:
                // TODO: implementar algoritmo Sign
                ESP_LOGW(TAG, "Algoritmo SIGN no implementado aún");
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
            case TICO:
                // TODO: implementar algoritmo Tico
                ESP_LOGW(TAG, "Algoritmo TICO no implementado aún");
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/*
 * Tarea de prueba para parada de emergencia.
 * Cada 5 ticks alterna emergency_stop para simular sensor ultrasónico.
 */
void EmergencyTestTask(void *param) {
    int tick_ms = BASE_TICK_MS;
    if (g_channel) {
        tick_ms = BASE_TICK_MS * g_channel->length / 100;
    }
    if (tick_ms < 50) tick_ms = 50;

    for (;;) {
        // Esperar 5 ticks
        vTaskDelay(pdMS_TO_TICKS(tick_ms * 5));

        if (!emergency_stop) {
            ESP_LOGW(TAG, ">>> EMERGENCIA: Objeto detectado — ACTIVANDO parada <<<");
            emergency_stop = true;
            // Despertar cualquier ShipTask bloqueada
            xEventGroupSetBits(channel_event_group, BIT_SHIP_MOVED);
        } else {
            ESP_LOGW(TAG, ">>> EMERGENCIA: Despejado — DESACTIVANDO parada <<<");
            emergency_stop = false;
        }
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
    canal_start_sem = xSemaphoreCreateBinary();

    init_buttons();
 
    // Inicializar LCDs de colas (0x23 y 0x26)
    lcd_init();

    // Inicializar LCD del canal (0x27)
    lcd_channel_init();
 
    xTaskCreate(CreadorTask, "Creador", 4096, NULL, 2, NULL);
    xTaskCreate(CanalTask, "Canal", 4096, NULL, 1, &canal_task_handle);

    // Tarea de prueba: alterna emergency_stop cada 5 ticks
    xTaskCreate(EmergencyTestTask, "EmergTest", 2048, NULL, 1, NULL);

    init_uart();
    xTaskCreate(UartReceiverTask, "UartReceiver", 4096, NULL, 1, NULL);
}
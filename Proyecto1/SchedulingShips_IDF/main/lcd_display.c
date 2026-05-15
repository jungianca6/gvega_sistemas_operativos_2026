#include "lcd_display.h"
#include "ship.h"
#include "ready_queue.h"
#include "channel.h"

#include "HD44780.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/* ---------- DIRECCIONES I2C ---------- */
#define LCD_ADDR_IZQ     0x23
#define LCD_ADDR_DER     0x26
#define LCD_ADDR_CHANNEL 0x27
#define SDA_PIN  21
#define SCL_PIN  22

/* ---------- CANAL: CONFIGURACIÓN ---------- */
#define MAX_SHIPS_IN_CHANNEL 16

static const char *TAG = "LCD";

/* ---------- LCD INSTANCES ---------- */
static LCD_t lcd_izq;
static LCD_t lcd_der;
static LCD_t lcd_channel;

/* ---------- CUSTOM CHARACTERS ---------- */
// Ship patterns (Hex format for maximum compatibility)
uint8_t Patrol[]   = { 0x04, 0x0E, 0x1F, 0x0E, 0x04, 0x11, 0x1F, 0x0E };
uint8_t Fishing[]  = { 0x04, 0x0A, 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11 };
uint8_t Standard[] = { 0x04, 0x06, 0x07, 0x07, 0x04, 0x15, 0x1F, 0x0E };
uint8_t Lock[]     = { 0x0E, 0x11, 0x11, 0x1F, 0x1B, 0x1B, 0x1F, 0x00 };
uint8_t Bell[]     = { 0x04, 0x0E, 0x0E, 0x0E, 0x1F, 0x00, 0x04, 0x00 };

/* ---------- CANAL: ESTADO COMPARTIDO ---------- */

static bool sign_active = false;
static Direction sign_dir = LEFT;

/*
 * Slot que representa un barco activo dentro del canal.
 */
typedef struct {
    Ship* ship;     // puntero al barco (NULL = slot libre)
    int col;        // columna actual en el LCD (0-15)
    int row;        // 0 = línea 1 (izq→der), 1 = línea 2 (der→izq)
    bool active;    // true si el slot está en uso
} ChannelSlot;

static ChannelSlot channel_slots[MAX_SHIPS_IN_CHANNEL];
static SemaphoreHandle_t channel_mutex;   // protege channel_slots[]
EventGroupHandle_t channel_event_group;   // señalización sin busy-wait

/* ================================================================
 *                    QUEUE LCDs (0x23, 0x26)
 * ================================================================ */

void lcd_init(void) {
    ESP_LOGI(TAG, "Iniciando inicializacion de LCDs...");

    // Initialize queue LCDs
    LCD_init(&lcd_izq, LCD_ADDR_IZQ, SDA_PIN, SCL_PIN, 16, 2);
    LCD_init(&lcd_der, LCD_ADDR_DER, SDA_PIN, SCL_PIN, 16, 2);

    // Create custom characters in both LCDs
    LCD_createChar(&lcd_izq, 1, Patrol);
    LCD_createChar(&lcd_izq, 2, Fishing);
    LCD_createChar(&lcd_izq, 3, Standard);

    LCD_createChar(&lcd_der, 1, Patrol);
    LCD_createChar(&lcd_der, 2, Fishing);
    LCD_createChar(&lcd_der, 3, Standard);

    LCD_clearScreen(&lcd_izq);
    LCD_clearScreen(&lcd_der);

    ESP_LOGI(TAG, "LCDs de colas inicializados (0x23 y 0x26) correctamente");
}

/*
 * Helper: dibuja hasta 4 barcos de una cola en un LCD.
 *
 * fill_from_right=true:  primer barco en col 15, llenando hacia la izquierda (cola izquierda)
 * fill_from_right=false: primer barco en col 0, llenando hacia la derecha (cola derecha)
 */
static void build_line(QueueShip *queue, LCD_t *lcd, int row, bool fill_from_right) {
    ESP_LOGD(TAG, "Dibujando cola en LCD addr: 0x%02X, row: %d, fill_right=%d", lcd->addr, row, fill_from_right);
    
    // Clear the specific line
    LCD_setCursor(lcd, 0, row);
    for (int i = 0; i < 16; i++) {
        LCD_writeChar(lcd, ' ');
    }

    if (queue == NULL || queue->front == NULL) {
        return;
    }

    Node *cur = queue->front;
    int count = 0;
    int pos = fill_from_right ? 15 : 0;
    int step = fill_from_right ? -1 : 1;

    while (cur && count < 4) {
        LCD_setCursor(lcd, pos, row);
        char c;
        switch (cur->ship->type) {
            case PATROL:   c = 1; break;
            case FISHING:  c = 2; break;
            case STANDARD: c = 3; break;
            default:       c = '?'; break;
        }
        LCD_writeChar(lcd, (unsigned char)c);
        pos += step;
        count++;
        cur = cur->next;
    }
}

void lcd_mostrar_colas(QueueShip *izq, QueueShip *der) {
    ESP_LOGI(TAG, "Actualizando pantallas de colas...");
    
    // LCD 0x23, row 0: primer barco en col 15, llenando hacia la izquierda
    build_line(izq, &lcd_izq, 0, true);

    // LCD 0x26, row 1: primer barco en col 0, llenando hacia la derecha
    build_line(der, &lcd_der, 1, false);
}

/* ================================================================
 *                  CHANNEL LCD (0x27) — INIT
 * ================================================================ */

void lcd_channel_init(void) {
    ESP_LOGI(TAG, "Iniciando LCD del canal (0x27)...");

    LCD_init(&lcd_channel, LCD_ADDR_CHANNEL, SDA_PIN, SCL_PIN, 16, 2);

    LCD_createChar(&lcd_channel, 1, Patrol);
    LCD_createChar(&lcd_channel, 2, Fishing);
    LCD_createChar(&lcd_channel, 3, Standard);
    LCD_createChar(&lcd_channel, 4, Lock);
    LCD_createChar(&lcd_channel, 5, Bell);

    LCD_clearScreen(&lcd_channel);

    // Inicializar slots vacíos
    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
        channel_slots[i].ship = NULL;
        channel_slots[i].col = -1;
        channel_slots[i].row = -1;
        channel_slots[i].active = false;
    }

    channel_mutex = xSemaphoreCreateMutex();
    channel_event_group = xEventGroupCreate();

    ESP_LOGI(TAG, "LCD del canal (0x27) inicializado correctamente");
}

/* ================================================================
 *           CHANNEL LCD — OPERACIONES CON SLOTS
 * ================================================================ */

/*
 * Retorna el carácter custom para un tipo de barco.
 */
static char ship_char(ShipType type) {
    switch (type) {
        case PATROL:   return 1;
        case FISHING:  return 2;
        case STANDARD: return 3;
        default:       return '?';
    }
}

/*
 * Encuentra el slot asignado a un barco.
 * Debe llamarse con channel_mutex tomado.
 */
static ChannelSlot* find_slot(Ship* ship) {
    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
        if (channel_slots[i].active && channel_slots[i].ship == ship) {
            return &channel_slots[i];
        }
    }
    return NULL;
}

/* ================================================================
 *            CHANNEL LCD — FUNCIONES PÚBLICAS
 * ================================================================ */

void lcd_channel_refresh(void) {
    // Limpiar ambas líneas del LCD del canal
    LCD_setCursor(&lcd_channel, 0, 0);
    for (int i = 0; i < 16; i++) LCD_writeChar(&lcd_channel, ' ');
    LCD_setCursor(&lcd_channel, 0, 1);
    for (int i = 0; i < 16; i++) LCD_writeChar(&lcd_channel, ' ');

    // Redibujar cada barco activo
    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
        if (channel_slots[i].active) {
            int col = channel_slots[i].col;
            int row = channel_slots[i].row;
            if (col >= 0 && col < 16) {
                LCD_setCursor(&lcd_channel, col, row);
                LCD_writeChar(&lcd_channel, (unsigned char)ship_char(channel_slots[i].ship->type));
            }
        }
    }

    // Dibujar señal si está activa
    if (sign_active) {
        if (sign_dir == LEFT) {
            LCD_setCursor(&lcd_channel, 0, 1);
            LCD_writeChar(&lcd_channel, 5); // Bell
        } else {
            LCD_setCursor(&lcd_channel, 15, 0);
            LCD_writeChar(&lcd_channel, 5); // Bell
        }
    }
}

int lcd_channel_place_ship(Ship* ship, int row) {
    int entry_col = (row == 0) ? 0 : 15;

    xSemaphoreTake(channel_mutex, portMAX_DELAY);

    // Buscar un slot libre
    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
        if (!channel_slots[i].active) {
            channel_slots[i].ship = ship;
            channel_slots[i].col = entry_col;
            channel_slots[i].row = row;
            channel_slots[i].active = true;

            ship->channel_col = entry_col;

            ESP_LOGI(TAG, "Canal: barco %d colocado en row=%d, col=%d",
                     ship->id, row, entry_col);

            lcd_channel_refresh();
            xSemaphoreGive(channel_mutex);
            return entry_col;
        }
    }

    xSemaphoreGive(channel_mutex);
    ESP_LOGE(TAG, "Canal: no hay slots libres para barco %d", ship->id);
    return -1;
}

void lcd_channel_remove_ship(Ship* ship) {
    xSemaphoreTake(channel_mutex, portMAX_DELAY);

    ChannelSlot* slot = find_slot(ship);
    if (slot) {
        ESP_LOGI(TAG, "Canal: barco %d removido de row=%d, col=%d",
                 ship->id, slot->row, slot->col);
        slot->ship = NULL;
        slot->col = -1;
        slot->row = -1;
        slot->active = false;
    }

    lcd_channel_refresh();
    xSemaphoreGive(channel_mutex);

    // Señalar que hubo un cambio (desbloquear ships esperando y CanalTask)
    xEventGroupSetBits(channel_event_group, BIT_SHIP_MOVED);
}

void lcd_channel_advance_ship(Ship* ship) {
    xSemaphoreTake(channel_mutex, portMAX_DELAY);

    ChannelSlot* slot = find_slot(ship);
    if (slot) {
        // Mover según dirección
        if (slot->row == 0) {
            // Izquierda → Derecha: avanzar columnas positivas
            slot->col += ship->speed;
            if (slot->col > 15) slot->col = 15;
        } else {
            // Derecha → Izquierda: avanzar columnas negativas
            slot->col -= ship->speed;
            if (slot->col < 0) slot->col = 0;
        }
        ship->channel_col = slot->col;

        ESP_LOGD(TAG, "Canal: barco %d avanzó a col=%d (speed=%d)",
                 ship->id, slot->col, ship->speed);

        lcd_channel_refresh();
    }

    xSemaphoreGive(channel_mutex);

    // Señalar que un barco se movió (desbloquear colisiones y chequeos de entrada)
    xEventGroupSetBits(channel_event_group, BIT_SHIP_MOVED);
}

bool lcd_check_pos(Ship* ship) {
    bool safe = true;

    xSemaphoreTake(channel_mutex, portMAX_DELAY);

    ChannelSlot* my_slot = find_slot(ship);
    if (!my_slot) {
        xSemaphoreGive(channel_mutex);
        return true; // Si no está en canal, no hay colisión
    }

    int my_row = my_slot->row;
    int my_col = my_slot->col;
    int speed = ship->speed;

    // Calcular la columna destino
    int next_col;
    if (my_row == 0) {
        next_col = my_col + speed;  // moviendo a la derecha
    } else {
        next_col = my_col - speed;  // moviendo a la izquierda
    }

    // Verificar contra todos los otros barcos en la misma fila
    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
        if (!channel_slots[i].active || channel_slots[i].ship == ship) continue;
        if (channel_slots[i].row != my_row) continue;

        int other_col = channel_slots[i].col;

        if (my_row == 0) {
            // Moviendo a la derecha: colisión si next_col >= other_col
            // (y el otro barco está delante de nosotros)
            if (other_col > my_col && next_col >= other_col) {
                safe = false;
                ESP_LOGD(TAG, "Colisión: barco %d (col %d→%d) vs barco %d (col %d)",
                         ship->id, my_col, next_col,
                         channel_slots[i].ship->id, other_col);
                break;
            }
        } else {
            // Moviendo a la izquierda: colisión si next_col <= other_col
            // (y el otro barco está delante de nosotros)
            if (other_col < my_col && next_col <= other_col) {
                safe = false;
                ESP_LOGD(TAG, "Colisión: barco %d (col %d→%d) vs barco %d (col %d)",
                         ship->id, my_col, next_col,
                         channel_slots[i].ship->id, other_col);
                break;
            }
        }
    }

    xSemaphoreGive(channel_mutex);
    return safe;
}


bool lcd_channel_entry_free(int row) {
    int entry_col = (row == 0) ? 0 : 15;
    bool free = true;

    xSemaphoreTake(channel_mutex, portMAX_DELAY);

    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
        if (channel_slots[i].active && channel_slots[i].row == row &&
            channel_slots[i].col == entry_col) {
            free = false;
            break;
        }
    }

    xSemaphoreGive(channel_mutex);
    return free;
}

bool lcd_channel_is_empty(void) {
    bool empty = true;

    xSemaphoreTake(channel_mutex, portMAX_DELAY);

    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
        if (channel_slots[i].active) {
            empty = false;
            break;
        }
    }

    xSemaphoreGive(channel_mutex);
    return empty;
}

int lcd_channel_evacuate_all(Ship** out_ships, int max_ships) {
    int count = 0;

    xSemaphoreTake(channel_mutex, portMAX_DELAY);

    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL && count < max_ships; i++) {
        if (channel_slots[i].active) {
            out_ships[count++] = channel_slots[i].ship;
            ESP_LOGI(TAG, "Evacuando barco %d de row=%d, col=%d",
                     channel_slots[i].ship->id, channel_slots[i].row, channel_slots[i].col);
            channel_slots[i].ship = NULL;
            channel_slots[i].col = -1;
            channel_slots[i].row = -1;
            channel_slots[i].active = false;
        }
    }

    lcd_channel_refresh();
    xSemaphoreGive(channel_mutex);
    return count;
}

void lcd_channel_show_lock(void) {
    LCD_setCursor(&lcd_channel, 8, 1);
    LCD_writeChar(&lcd_channel, (unsigned char)4);
    ESP_LOGI(TAG, "Canal BLOQUEADO - candado mostrado");
}

void lcd_channel_clear_lock(void) {
    LCD_setCursor(&lcd_channel, 8, 1);
    LCD_writeChar(&lcd_channel, ' ');
    ESP_LOGI(TAG, "Canal DESBLOQUEADO - candado removido");
}

int lcd_channel_evacuate_dir(Ship** out_ships, int max_ships, int row) {
    int count = 0;
    xSemaphoreTake(channel_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL && count < max_ships; i++) {
        if (channel_slots[i].active && channel_slots[i].row == row) {
            out_ships[count++] = channel_slots[i].ship;
            channel_slots[i].ship = NULL;
            channel_slots[i].col = -1;
            channel_slots[i].row = -1;
            channel_slots[i].active = false;
        }
    }
    lcd_channel_refresh();
    xSemaphoreGive(channel_mutex);
    if (count > 0) {
        xEventGroupSetBits(channel_event_group, BIT_SHIP_MOVED);
    }
    return count;
}

bool lcd_channel_restore_ship(Ship* ship, int row, int col) {
    xSemaphoreTake(channel_mutex, portMAX_DELAY);
    bool placed = false;
    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
        if (!channel_slots[i].active) {
            channel_slots[i].ship = ship;
            channel_slots[i].col = col;
            channel_slots[i].row = row;
            channel_slots[i].active = true;
            ship->channel_col = col;
            placed = true;
            break;
        }
    }
    lcd_channel_refresh();
    xSemaphoreGive(channel_mutex);
    if (placed) {
        xEventGroupSetBits(channel_event_group, BIT_SHIP_MOVED);
    }
    return placed;
}

void lcd_channel_set_sign(Direction dir) {
    xSemaphoreTake(channel_mutex, portMAX_DELAY);
    sign_active = true;
    sign_dir = dir;
    lcd_channel_refresh();
    xSemaphoreGive(channel_mutex);
}

void lcd_channel_clear_sign(void) {
    xSemaphoreTake(channel_mutex, portMAX_DELAY);
    sign_active = false;
    lcd_channel_refresh();
    xSemaphoreGive(channel_mutex);
}
#include "lcd_display.h"
#include "ship.h"
#include "ready_queue.h"

#include "HD44780.h"

#include "esp_log.h"
#include <string.h>
#include <stdio.h>

#define LCD_ADDR 0x26
#define SDA_PIN  21
#define SCL_PIN  22

static const char *TAG = "LCD";
static LCD_t main_lcd;

// ---------------- INIT ----------------
void lcd_init(void) {
    LCD_init(&main_lcd, LCD_ADDR, SDA_PIN, SCL_PIN, 16, 2);
    LCD_clearScreen(&main_lcd);
    ESP_LOGI(TAG, "LCD inicializado correctamente");
}

// ---------------- UTIL ----------------
static char shipTypeToChar(ShipType type) {
    switch (type) {
    case STANDARD:   return 'S';
    case FISHING: return 'F';
    case PATROL: return 'P';
    default:       return '?';
    }
}

static void build_line(QueueShip *queue, const char *prefix, char *buf) {
    memset(buf, ' ', 16);
    buf[16] = '\0';

    int prefix_len = strlen(prefix);
    memcpy(buf, prefix, prefix_len);

    int pos = prefix_len;

    Node *cur = queue->front;
    while (cur && pos < 15) {
        buf[pos++] = shipTypeToChar(cur->ship->type);
        if (pos < 15) buf[pos++] = ' ';
        cur = cur->next;
    }
}

// ---------------- DISPLAY ----------------
void lcd_mostrar_colas(QueueShip *izq, QueueShip *der) {
    char linea0[17];
    char linea1[17];

    build_line(izq, "IZQ:", linea0);
    build_line(der, "DER:", linea1);

    LCD_setCursor(&main_lcd, 0, 0);
    LCD_writeStr(&main_lcd, linea0);

    LCD_setCursor(&main_lcd, 0, 1);
    LCD_writeStr(&main_lcd, linea1);

    ESP_LOGI(TAG, "LCD -> [%s] [%s]", linea0, linea1);
}
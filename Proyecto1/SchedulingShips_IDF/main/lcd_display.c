#include "lcd_display.h"
#include "ship.h"
#include "ready_queue.h"

#include "HD44780.h"

#include "esp_log.h"
#include <string.h>
#include <stdio.h>

#define LCD_ADDR 0x27
#define SDA_PIN  6
#define SCL_PIN  7

static const char *TAG = "LCD";

// ---------------- INIT ----------------
void lcd_init(void) {
    LCD_init(LCD_ADDR, SDA_PIN, SCL_PIN, 16, 2);

    LCD_clearScreen();

    ESP_LOGI(TAG, "LCD inicializado correctamente");
}

// ---------------- UTIL ----------------
static char shipTypeToChar(ShipType type) {
    switch (type) {
    case NORMAL:   return 'N';
    case PESQUERO: return 'P';
    case PATRULLA: return 'T';
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

    LCD_setCursor(0, 0);
    LCD_writeStr(linea0);

    LCD_setCursor(0, 1);
    LCD_writeStr(linea1);

    ESP_LOGI(TAG, "LCD -> [%s] [%s]", linea0, linea1);
}
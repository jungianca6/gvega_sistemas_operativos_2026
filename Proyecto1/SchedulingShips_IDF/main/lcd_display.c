#include "lcd_display.h"
#include "ship.h"
#include "ready_queue.h"

#include "HD44780.h"

#include "esp_log.h"
#include <string.h>
#include <stdio.h>

#define LCD_ADDR_IZQ 0x23
#define LCD_ADDR_DER 0x26
#define SDA_PIN  21
#define SCL_PIN  22

static const char *TAG = "LCD";
static LCD_t lcd_izq;
static LCD_t lcd_der;

// Ship patterns (Hex format for maximum compatibility)
uint8_t Patrol[]   = { 0x04, 0x0E, 0x1F, 0x0E, 0x04, 0x11, 0x1F, 0x0E };
uint8_t Fishing[]  = { 0x04, 0x0A, 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11 };
uint8_t Standard[] = { 0x04, 0x06, 0x07, 0x07, 0x04, 0x15, 0x1F, 0x0E };

// ---------------- INIT ----------------
void lcd_init(void) {
    ESP_LOGI(TAG, "Iniciando inicializacion de LCDs...");

    // Initialize both LCDs
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

    // Splash screen to verify they work
    //LCD_setCursor(&lcd_izq, 0, 0);
    //LCD_writeStr(&lcd_izq, "IZQ: 0x23 OK");
    //LCD_setCursor(&lcd_der, 0, 1);
    //LCD_writeStr(&lcd_der, "DER: 0x26 OK");

    //vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "LCDs inicializados (0x23 y 0x26) correctamente");
}

// ---------------- UTIL ----------------
static void build_line(QueueShip *queue, LCD_t *lcd, int row) {
    ESP_LOGD(TAG, "Dibujando cola en LCD addr: 0x%02X, row: %d", lcd->addr, row);
    
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
    int pos = 15; // The first ship in queue will be in the last matrix (column 15)

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
        pos--;
        count++;
        cur = cur->next;
    }
}

// ---------------- DISPLAY ----------------
void lcd_mostrar_colas(QueueShip *izq, QueueShip *der) {
    ESP_LOGI(TAG, "Actualizando pantallas...");
    
    // Address 0x23 uses only the first line (row 0)
    build_line(izq, &lcd_izq, 0);

    // Address 0x26 uses only the second line (row 1)
    build_line(der, &lcd_der, 1);
}
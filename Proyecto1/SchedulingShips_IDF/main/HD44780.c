#include "HD44780.h"
#include <driver/i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "rom/ets_sys.h"

// LCD module defines
#define LCD_LINEONE             0x00        // start of line 1
#define LCD_LINETWO             0x40        // start of line 2
#define LCD_LINETHREE           0x14        // start of line 3
#define LCD_LINEFOUR            0x54        // start of line 4

#define LCD_BACKLIGHT           0x08
#define LCD_ENABLE              0x04
#define LCD_COMMAND             0x00
#define LCD_WRITE               0x01

#define LCD_SET_DDRAM_ADDR      0x80
#define LCD_READ_BF             0x40

// LCD instructions
#define LCD_CLEAR               0x01        // replace all characters with ASCII 'space'
#define LCD_HOME                0x02        // return cursor to first position on first line
#define LCD_ENTRY_MODE          0x06        // shift cursor from left to right on read/write
#define LCD_DISPLAY_OFF         0x08        // turn display off
#define LCD_DISPLAY_ON          0x0C        // display on, cursor off, don't blink character
#define LCD_FUNCTION_RESET      0x30        // reset the LCD
#define LCD_FUNCTION_SET_4BIT   0x28        // 4-bit data, 2-line display, 5 x 7 font
#define LCD_SET_CURSOR          0x80        // set cursor position

static const char* TAG = "LCD Driver";
static bool i2c_initialized = false;

static void LCD_writeNibble(LCD_t* lcd, uint8_t nibble, uint8_t mode);
static void LCD_writeByte(LCD_t* lcd, uint8_t data, uint8_t mode);

static esp_err_t I2C_init(uint8_t sda, uint8_t scl)
{
    if (i2c_initialized) {
        return ESP_OK;
    }

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000
    };
    i2c_param_config(I2C_NUM_0, &conf);
    esp_err_t err = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        i2c_initialized = true;
        return ESP_OK;
    }
    return err;
}

void LCD_init(LCD_t* lcd, uint8_t addr, uint8_t sdaPin, uint8_t sclPin, uint8_t cols, uint8_t rows)
{
    lcd->addr = addr;
    lcd->sda_pin = sdaPin;
    lcd->scl_pin = sclPin;
    lcd->cols = cols;
    lcd->rows = rows;

    I2C_init(lcd->sda_pin, lcd->scl_pin);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Reset sequence
    LCD_writeNibble(lcd, LCD_FUNCTION_RESET, LCD_COMMAND);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    LCD_writeNibble(lcd, LCD_FUNCTION_RESET, LCD_COMMAND);
    ets_delay_us(200);
    LCD_writeNibble(lcd, LCD_FUNCTION_RESET, LCD_COMMAND);
    LCD_writeNibble(lcd, LCD_FUNCTION_SET_4BIT, LCD_COMMAND);
    ets_delay_us(80);

    LCD_writeByte(lcd, LCD_FUNCTION_SET_4BIT, LCD_COMMAND);
    ets_delay_us(80);

    LCD_writeByte(lcd, LCD_CLEAR, LCD_COMMAND);
    vTaskDelay(2 / portTICK_PERIOD_MS);

    LCD_writeByte(lcd, LCD_ENTRY_MODE, LCD_COMMAND);
    ets_delay_us(80);

    LCD_writeByte(lcd, LCD_DISPLAY_ON, LCD_COMMAND);
}

void LCD_setCursor(LCD_t* lcd, uint8_t col, uint8_t row)
{
    if (row >= lcd->rows) {
        ESP_LOGW(TAG, "Row %d out of range (0-%d)", row, lcd->rows - 1);
        row = lcd->rows - 1;
    }
    uint8_t row_offsets[] = {LCD_LINEONE, LCD_LINETWO, LCD_LINETHREE, LCD_LINEFOUR};
    LCD_writeByte(lcd, LCD_SET_DDRAM_ADDR | (col + row_offsets[row]), LCD_COMMAND);
}

void LCD_writeChar(LCD_t* lcd, char c)
{
    LCD_writeByte(lcd, c, LCD_WRITE);
}

void LCD_writeStr(LCD_t* lcd, char* str)
{
    while (*str) {
        LCD_writeChar(lcd, *str++);
    }
}

void LCD_home(LCD_t* lcd)
{
    LCD_writeByte(lcd, LCD_HOME, LCD_COMMAND);
    vTaskDelay(2 / portTICK_PERIOD_MS);
}

void LCD_clearScreen(LCD_t* lcd)
{
    LCD_writeByte(lcd, LCD_CLEAR, LCD_COMMAND);
    vTaskDelay(2 / portTICK_PERIOD_MS);
}

static void LCD_writeNibble(LCD_t* lcd, uint8_t nibble, uint8_t mode)
{
    uint8_t data = (nibble & 0xF0) | LCD_BACKLIGHT;
    if (mode) data |= (1 << 0); // RS = P0

    // Enable HIGH
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (lcd->addr << 1) | I2C_MASTER_WRITE, 1);
    i2c_master_write_byte(cmd, data | LCD_ENABLE, 1);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000/portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    ets_delay_us(1);

    // Enable LOW
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (lcd->addr << 1) | I2C_MASTER_WRITE, 1);
    i2c_master_write_byte(cmd, data & ~LCD_ENABLE, 1);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000/portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    ets_delay_us(50);
}

static void LCD_writeByte(LCD_t* lcd, uint8_t data, uint8_t mode)
{
    LCD_writeNibble(lcd, data & 0xF0, mode);
    LCD_writeNibble(lcd, (data << 4) & 0xF0, mode);
}
void LCD_createChar(LCD_t* lcd, uint8_t location, uint8_t charmap[])
{
    location &= 0x07; // We only have 8 locations 0-7
    LCD_writeByte(lcd, 0x40 | (location << 3), LCD_COMMAND);
    for (int i = 0; i < 8; i++) {
        LCD_writeByte(lcd, charmap[i], LCD_WRITE);
    }
}

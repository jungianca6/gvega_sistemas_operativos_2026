#ifndef HD44780
#define HD44780

#include <stdint.h>

/**
 * @brief Structure to represent an LCD instance.
 */
typedef struct {
    uint8_t addr;      // I2C address
    uint8_t sda_pin;   // SDA GPIO pin
    uint8_t scl_pin;   // SCL GPIO pin
    uint8_t cols;      // Number of columns
    uint8_t rows;      // Number of rows
} LCD_t;

/**
 * @brief Initialize the LCD.
 */
void LCD_init(LCD_t* lcd, uint8_t addr, uint8_t sdaPin, uint8_t sclPin, uint8_t cols, uint8_t rows);

/**
 * @brief Set the cursor position.
 */
void LCD_setCursor(LCD_t* lcd, uint8_t col, uint8_t row);

/**
 * @brief Return cursor to home position.
 */
void LCD_home(LCD_t* lcd);

/**
 * @brief Clear the entire screen.
 */
void LCD_clearScreen(LCD_t* lcd);

/**
 * @brief Write a single character.
 */
void LCD_writeChar(LCD_t* lcd, char c);

/**
 * @brief Write a null-terminated string.
 */
void LCD_writeStr(LCD_t* lcd, char* str);

/**
 * @brief Create a custom character in CGRAM.
 */
void LCD_createChar(LCD_t* lcd, uint8_t location, uint8_t charmap[]);

#endif

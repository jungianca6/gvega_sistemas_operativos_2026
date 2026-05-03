
#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H
#include "ready_queue.h"

/*
 * Inicializa el LCD HD44780 via I2C.
 * Debe llamarse una sola vez en app_main antes de usar lcd_mostrar_colas().
 */
void lcd_init(void);

/*
 * Actualiza el LCD con el estado actual de ambas colas.
 * Fila 0: cola izquierda
 * Fila 1: cola derecha
 */
void lcd_mostrar_colas(QueueShip *izq, QueueShip *der);
#endif

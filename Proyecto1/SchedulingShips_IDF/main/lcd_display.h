
#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "ready_queue.h"
#include "ship.h"
#include <stdbool.h>
#include "freertos/event_groups.h"

/* Bit del event group: algún barco se movió (desbloquea colisiones y chequeos de entrada) */
#define BIT_SHIP_MOVED  (1 << 0)

/*
 * Inicializa los LCDs de colas (0x23 y 0x26).
 * Debe llamarse una sola vez en app_main.
 */
void lcd_init(void);

/*
 * Inicializa el LCD del canal (0x27).
 * Debe llamarse después de lcd_init().
 */
void lcd_channel_init(void);

/*
 * Actualiza los LCDs de colas con el estado actual.
 * Fila 0: cola izquierda (LCD 0x23)
 * Fila 1: cola derecha (LCD 0x26)
 */
void lcd_mostrar_colas(QueueShip *izq, QueueShip *der);

/*
 * Coloca un barco en la posición de entrada del canal LCD.
 * row=0 para izq→der (col 0), row=1 para der→izq (col 15).
 * Retorna la columna de inicio asignada.
 */
int lcd_channel_place_ship(Ship* ship, int row);

/*
 * Remueve un barco del canal LCD y libera su slot.
 */
void lcd_channel_remove_ship(Ship* ship);

/*
 * Avanza un barco según su velocidad en el canal LCD.
 * Actualiza la posición y redibuja.
 */
void lcd_channel_advance_ship(Ship* ship);

/*
 * Redibuja completamente el LCD del canal basado en las posiciones actuales.
 */
void lcd_channel_refresh(void);

/*
 * Verifica si mover el barco por su velocidad causaría colisión.
 * Retorna true si el movimiento es seguro, false si colisionaría.
 */
bool lcd_check_pos(Ship* ship);

/*
 * Verifica si la columna de entrada de un row está libre.
 * row=0: verifica col 0. row=1: verifica col 15.
 */
bool lcd_channel_entry_free(int row);

/*
 * Verifica si hay barcos activos en el canal.
 * Retorna true si el canal está vacío.
 */
bool lcd_channel_is_empty(void);

/* Event group global para sincronización del canal */
extern EventGroupHandle_t channel_event_group;

#endif

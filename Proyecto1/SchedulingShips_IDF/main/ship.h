#ifndef BARCO_H
#define BARCO_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdbool.h>

/*
 * Enumeración que representa los tipos de barcos disponibles
 * en el sistema.
 *
 * STANDARD: barco estándar.
 * FISHING: barco tipo pesquero.
 * PATROL: barco de patrulla.
 */
typedef enum {
    STANDARD,
    FISHING,
    PATROL
} ShipType;

/*
 * Enumeración que representa la dirección hacia donde se dirige
 * el barco dentro del canal.
 */
typedef enum {
    LEFT,
    RIGHT
} Direction;

/*
 * Estructura principal que representa un barco.
 *
 * Algunos campos como time_remaining, deadline y priority quedan
 * preparados para futuras implementaciones de algoritmos de
 * planificación.
 */
typedef struct Ship {
    ShipType type;
    int id;
    int direction;
    int burst_time;
    int time_remaining;
    int speed;
    int deadline;
    int priority;
    bool preempted;
    int saved_channel_col; // Donde estaba el barco antes de ser preemptado
    SemaphoreHandle_t sem; // Semáforo para controlar el turno
    int channel_col;              // Posición actual en el LCD del canal (0-15), -1 si no está en canal
    TaskHandle_t task_handle;     // Handle de la tarea FreeRTOS de este barco
} Ship;

const char* shipTypeToString(ShipType type);
const char* dirToString(Direction dir);
void inicializar_barco(Ship* barco, int id, ShipType type, Direction dir,
                       int speed, int burst, int priority, int deadline);

#endif // BARCO_H
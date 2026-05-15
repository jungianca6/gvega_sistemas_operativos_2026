#include "ship.h"
#include <stdio.h>

/*
 * Inicializa una estructura Ship con los datos recibidos.
 *
 * Parámetros:
 * - barco: puntero a la estructura Ship que será inicializada.
 * - id: identificador único del barco.
 * - type: tipo de barco.
 * - dir: dirección del barco.
 *
 * La velocidad se asigna dependiendo del tipo de barco.
 */
void inicializar_barco(
    Ship* barco,
    int id,
    ShipType type,
    Direction dir,
    int speed,
    int burst,
    int priority,
    int deadline
) {
    barco->id = id;
    barco->type = type;
    barco->direction = dir;

    barco->speed = speed;

    barco->burst_time = burst;
    barco->time_remaining = burst;

    barco->priority = priority;
    barco->deadline = deadline;
    barco->saved_channel_col = -1;
    barco->sem = xSemaphoreCreateBinary();

    barco->channel_col = -1;
    barco->task_handle = NULL;
    barco->preempted = false;
}

/*
 * Devuelve una abreviatura del tipo de barco.
 *
 * STD = Standard
 * FHS = Fishing
 * PTR = Patrol
 */
const char* shipTypeToString(ShipType type) {
    switch(type) {
    case STANDARD: return "STD";
    case FISHING: return "FHS";
    case PATROL: return "PTR";
    default: return "?";
    }
}

/*
 * Devuelve una abreviatura de la dirección del barco.
 *
 * L = Left
 * R = Right
 */
const char* dirToString(Direction dir) {
    switch(dir) {
    case LEFT: return "L";
    case RIGHT: return "R";
    default: return "?";
    }
}

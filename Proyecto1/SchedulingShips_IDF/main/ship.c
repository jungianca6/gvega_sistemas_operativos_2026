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
void inicializar_barco(Ship* barco, int id, ShipType type, Direction dir) {
    barco->id = id;
    barco->type = type;
    barco->direction = dir;

    switch (type) {
        case NORMAL:
            barco->speed = 1.0;
            break;
        case PESQUERO:
            barco->speed = 2.0;
            break;
        case PATRULLA:
            barco->speed = 3.0;
            break;
        default:
            barco->speed = 1.0;
        break;
    }
}

/*
 * Devuelve una abreviatura del tipo de barco.
 *
 * N  = Normal
 * PE = Pesquero
 * PA = Patrulla
 */
const char* shipTypeToString(ShipType type) {
    switch(type) {
    case NORMAL: return "N";
    case PESQUERO: return "PE";
    case PATRULLA: return "PA";
    default: return "?";
    }
}

/*
 * Devuelve una abreviatura de la dirección del barco.
 *
 * IZQ = Izquierda
 * DER = Derecha
 */
const char* dirToString(Direction dir) {
    switch(dir) {
    case IZQUIERDA: return "IZQ";
    case DERECHA: return "DER";
    default: return "?";
    }
}

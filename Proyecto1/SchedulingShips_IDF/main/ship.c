#include "ship.h"
#include <stdio.h>

// Función para inicializar un barco con los valores dados
void inicializar_barco(Ship* barco, int id, ShipType type) {
    barco->id = id;
    barco->type = type;

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


void printShip(const Ship* b) {
    printf("Tipo de barco: ");

    switch(b->type) {
        case NORMAL:
            printf("Normal");
            break;
        case PESQUERO:
            printf("Pesquero");
            break;
        case PATRULLA:
            printf("Patrulla");
            break;
        default:
            printf("Desconocido");
            break;
    }
}
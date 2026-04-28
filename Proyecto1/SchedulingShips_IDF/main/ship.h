#ifndef BARCO_H
#define BARCO_H

typedef enum {
    NORMAL,
    PESQUERO,
    PATRULLA
} ShipType;

typedef struct Ship {
    ShipType type;
    int id;
    int direction;
    int time_remaining;
    int speed;
    int deadline;
    int priority;
} Ship;


void printShip(const Ship* b);
void inicializar_barco(Ship* barco, int id, ShipType type);

#endif // BARCO_H
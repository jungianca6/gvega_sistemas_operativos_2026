#ifndef BARCO_H
#define BARCO_H
/*
 * Enumeración que representa los tipos de barcos disponibles
 * en el sistema.
 *
 * NORMAL: barco estándar.
 * PESQUERO: barco tipo pesquero.
 * PATRULLA: barco de patrulla.
 */
typedef enum {
    NORMAL,
    PESQUERO,
    PATRULLA
} ShipType;

/*
 * Enumeración que representa la dirección hacia donde se dirige
 * el barco dentro del canal.
 */
typedef enum {
    IZQUIERDA,
    DERECHA
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
    int time_remaining;
    int speed;
    int deadline;
    int priority;
} Ship;

const char* shipTypeToString(ShipType type);
const char* dirToString(Direction dir);
void inicializar_barco(Ship* barco, int id, ShipType type, Direction dir);

#endif // BARCO_H
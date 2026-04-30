#ifndef READY_QUEUE_H
#define READY_QUEUE_H

#include "ship.h"
/*
 * Cantidad máxima de barcos permitidos por cola.
 */
#define MAX_QUEUE 4

/*
 * Nodo de la lista enlazada.
 *
 * Cada nodo almacena un puntero a un barco y una referencia
 * al siguiente nodo.
 */
typedef struct Node {
    Ship* ship;
    struct Node* next;
} Node;

/*
 * Cola de barcos implementada mediante lista enlazada.
 *
 * front: apunta al primer elemento de la cola.
 * rear: apunta al último elemento de la cola.
 * size: cantidad actual de elementos.
 * name: nombre usado para identificar la cola al imprimirla.
 */
typedef struct {
    Node* front;
    Node* rear;
    int size;
    char name[20];
} QueueShip;
/*
 * Inicializa una cola vacía con el nombre indicado.
 */
void initQueue(QueueShip* q, const char* name);
/*
 * Agrega un barco al final de la cola.
 *
 * Retorna:
 * - 1 si el barco se agregó correctamente.
 * - 0 si la cola está llena o si falla la reserva de memoria.
 */
int enqueue(QueueShip* q, Ship* s);
/*
 * Extrae el primer barco de la cola.
 *
 * Retorna:
 * - 1 si se extrajo un barco correctamente.
 * - 0 si la cola estaba vacía.
 */
int dequeue(QueueShip* q, Ship** s);
/*
 * Imprime el contenido actual de la cola.
 */
void printQueue(QueueShip* q);

#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ready_queue.h"

/*
 * Inicializa una cola de barcos.
 *
 * La cola empieza vacía, por lo que front y rear apuntan a NULL.
 */
void initQueue(QueueShip* q, const char* name) {
    q->front = q->rear = NULL;
    q->size = 0;
    strcpy(q->name, name);
}

/*
 * Inserta un barco al final de la cola.
 *
 * Primero verifica si la cola ya llegó al máximo permitido.
 * Luego reserva memoria para un nuevo nodo y lo enlaza al final.
 */
int enqueue(QueueShip* q, Ship* s) {
    if (q->size >= MAX_QUEUE) {
        printf("[%s] LLENA\n", q->name);
        return 0;
    }

    Node* n = malloc(sizeof(Node));
    if (!n) return 0;
    n->ship = s;
    n->next = NULL;

    if (!q->rear) {
        q->front = q->rear = n;
    } else {
        q->rear->next = n;
        q->rear = n;
    }

    q->size++;
    return 1;
}

/*
 * Extrae el primer barco de la cola.
 *
 * El barco extraído se devuelve mediante el parámetro doble puntero Ship**.
 * Después de extraer el nodo, se libera la memoria del nodo.
 *
 * Importante:
 * Esta función libera el nodo de la cola, pero NO libera el barco.
 * La memoria del barco debe liberarse después de ser usado.
 */
int dequeue(QueueShip* q, Ship** s) {
    if (!q->front) return 0;

    Node* temp = q->front;
    *s = temp->ship;

    q->front = temp->next;
    if (!q->front) q->rear = NULL;

    free(temp);
    q->size--;
    return 1;
}

/*
 * Imprime todos los barcos presentes en la cola.
 *
 * El formato impreso es:
 * [id-tipo-direccion] -> [id-tipo-direccion] -> NULL
 */
void printQueue(QueueShip* q) {
    printf("%s: ", q->name);

    Node* cur = q->front;
    while (cur) {
        printf("[%d-%s-%s] -> ",
        cur->ship->id,
        shipTypeToString(cur->ship->type),
        dirToString(cur->ship->direction));
        cur = cur->next;
    }

    printf("NULL\n");
}
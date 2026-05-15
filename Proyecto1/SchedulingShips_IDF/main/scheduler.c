#include "scheduler.h"
#include <stdlib.h>

static int goes_before(
    Ship* a,
    Ship* b,
    SchedulerType scheduler
) {

    switch (scheduler) {

    case SCHEDULER_PRIORITY:
        return a->priority < b->priority;

    case SCHEDULER_SJF:
        return a->burst_time < b->burst_time;

    case SCHEDULER_STRN:
        return a->time_remaining < b->time_remaining;

    case SCHEDULER_EDF:
        return a->deadline < b->deadline;

    case SCHEDULER_FCFS:
    case SCHEDULER_RR:
    default:
        return 0;
    }
}

int scheduler_enqueue_ordered(
    QueueShip* queue,
    Ship* ship,
    SchedulerType scheduler
) {

    if (!queue || !ship) {
        return 0;
    }

    if (queue->size >= MAX_QUEUE) {
        return 0;
    }

    if (
        scheduler == SCHEDULER_FCFS ||
        scheduler == SCHEDULER_RR
    ) {
        return enqueue(queue, ship);
    }

    Node* new_node = malloc(sizeof(Node));

    if (!new_node) return 0;

    new_node->ship = ship;
    new_node->next = NULL;

    if (!queue->front) {

        queue->front = queue->rear = new_node;
        queue->size++;

        return 1;
    }

    if (goes_before(ship, queue->front->ship, scheduler)) {

        new_node->next = queue->front;
        queue->front = new_node;

        queue->size++;

        return 1;
    }

    Node* current = queue->front;

    while (
        current->next &&
        !goes_before(
            ship,
            current->next->ship,
            scheduler
        )
    ) {
        current = current->next;
    }

    new_node->next = current->next;
    current->next = new_node;

    if (!new_node->next) {
        queue->rear = new_node;
    }

    queue->size++;

    return 1;
}

int scheduler_remove_ship(
    QueueShip* queue,
    Ship* ship
) {

    if (!queue || !queue->front) {
        return 0;
    }

    Node* current = queue->front;
    Node* previous = NULL;

    while (current) {

        if (current->ship == ship) {

            if (!previous) {
                queue->front = current->next;
            }
            else {
                previous->next = current->next;
            }

            if (queue->rear == current) {
                queue->rear = previous;
            }

            free(current);

            queue->size--;

            return 1;
        }

        previous = current;
        current = current->next;
    }

    return 0;
}

Ship* scheduler_select_next(
    QueueShip* queue,
    SchedulerType scheduler
) {

    if (!queue || !queue->front) {
        return NULL;
    }

    return queue->front->ship;
}

const char* scheduler_to_string(SchedulerType scheduler) {
    switch (scheduler) {
    case SCHEDULER_FCFS:
        return "FCFS";
    case SCHEDULER_RR:
        return "RR";
    case SCHEDULER_PRIORITY:
        return "PRIORITY";
    case SCHEDULER_SJF:
        return "SJF";
    case SCHEDULER_STRN:
        return "STRN";
    case SCHEDULER_EDF:
        return "EDF";
    default:
        return "UNKNOWN";
    }
}
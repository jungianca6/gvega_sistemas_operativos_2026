#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "ready_queue.h"

typedef enum {
    SCHEDULER_FCFS,
    SCHEDULER_RR,
    SCHEDULER_PRIORITY,
    SCHEDULER_SJF,
    SCHEDULER_STRN,
    SCHEDULER_EDF
} SchedulerType;

int scheduler_enqueue_ordered(
    QueueShip* queue,
    Ship* ship,
    SchedulerType scheduler
);

int scheduler_remove_ship(
    QueueShip* queue,
    Ship* ship
);

Ship* scheduler_select_next(
    QueueShip* queue,
    SchedulerType scheduler
);

const char* scheduler_to_string(SchedulerType scheduler);

#endif
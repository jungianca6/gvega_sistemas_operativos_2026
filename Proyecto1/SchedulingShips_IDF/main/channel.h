#ifndef CHANNEL_H
#define CHANNEL_H

#include "ready_queue.h"
#include "scheduler.h"
#include <stdbool.h>

/*
 * Enumeración que representa los métodos de control de flujo.
 *
 * FAIRNESS: según parámetro w.
 * SIGNAL: según intervalo de tiempo.
 * TICO: no hay control de flujo.
 */
typedef enum {
    FAIRNESS,
    SIGN,
    TICO
} FlowControl;

/*
 * Estructura principal que representa un canal.
 *
 * length: longitud del canal.
 * speed: velocidad de los barcos.
 * flow_control: método de control de flujo.
 * sign_interval: intervalo de tiempo para el cambio de señal.
 * w_parameter: parámetro W para el control de flujo.
 */
typedef struct Channel {
    int length;
    int speed;
    int flow_control;
    int sign_interval;
    int w_parameter;
    SchedulerType scheduler;
    int quantum_rr;
} Channel;


/**
 * Puntero global al canal principal.
 * Se establece con channel_set_global() después de init_channel().
 */
extern Channel* g_channel;

/** Variable global de parada de emergencia. Modificada por sensor ultrasónico. */
extern volatile bool emergency_stop;

void init_channel(Channel* channel, int length, int speed,
    int flow_control, int sign_interval, int w_parameter, SchedulerType scheduler, int quantum_rr);
void channel_set_global(Channel* ch);

#endif // CHANNEL_H
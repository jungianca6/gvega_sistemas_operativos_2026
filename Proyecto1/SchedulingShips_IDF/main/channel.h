#ifndef CHANNEL_H
#define CHANNEL_H

#include "ready_queue.h"

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
} Channel;


void init_channel(Channel* channel, int length, int speed, int flow_control, int sign_interval, int w_parameter);

#endif // CHANNEL_H
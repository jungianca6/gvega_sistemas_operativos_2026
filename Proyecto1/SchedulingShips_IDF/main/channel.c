#include "channel.h"

Channel* g_channel = NULL;

/*
 * Inicializa una estructura Channel con los datos recibidos.
 *
 * Parámetros:
 * - channel: puntero a la estructura Channel que será inicializada.
 * - length: longitud del canal.
 * - speed: velocidad de los barcos.
 * - flow_control: método de control de flujo.
 * - sign_interval: intervalo de tiempo para el cambio de señal.
 * - w_parameter: parámetro W para el control de flujo.
 *
 */
void init_channel(Channel* channel, int length, int speed, int flow_control, int sign_interval, int w_parameter) {
    channel->length = length;
    channel->speed = speed;
    channel->flow_control = flow_control;
    channel->sign_interval = sign_interval;
    channel->w_parameter = w_parameter;
}

void channel_set_global(Channel* ch) {
    g_channel = ch;
}


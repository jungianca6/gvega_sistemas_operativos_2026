#include <stdio.h>
#include <unistd.h>

#include "wolfPlotter.h"

#define PENPLTTR_PROC_PATH "/proc/penplttr-gpio"
#define PENPLTTR_CMD_BUF_SIZE 64

static FILE *proc_file = NULL;

int wolfPlotter_init(void)
{
    proc_file = fopen(PENPLTTR_PROC_PATH, "w");
    if (proc_file == NULL)
    {
        perror("wolfPlotter: failed to open " PENPLTTR_PROC_PATH);
        return -1;
    }

    printf("wolfPlotter: initialized successfully\n");
    return 0;
}

void wolfPlotter_cleanup(void)
{
    if (proc_file != NULL)
    {
        fclose(proc_file);
        proc_file = NULL;
    }

    printf("wolfPlotter: cleaned up\n");
}

int wolfPlotter_setPin(unsigned int pin, unsigned int value)
{
    char cmd[PENPLTTR_CMD_BUF_SIZE];

    if (proc_file == NULL)
    {
        fprintf(stderr, "wolfPlotter: not initialized, call wolfPlotter_init() first\n");
        return -1;
    }

    if (pin > 27)
    {
        fprintf(stderr, "wolfPlotter: invalid pin %u (valid range: 0-27)\n", pin);
        return -1;
    }

    if (value != 0 && value != 1)
    {
        fprintf(stderr, "wolfPlotter: invalid value %u (must be 0 or 1)\n", value);
        return -1;
    }

    snprintf(cmd, sizeof(cmd), "%u,%u\n", pin, value);

    if (fputs(cmd, proc_file) == EOF)
    {
        perror("wolfPlotter: failed to write to proc file");
        return -1;
    }
    fflush(proc_file);

    return 0;
}

int wolfPlotter_blink(unsigned int pin, unsigned int count, unsigned int delay_ms)
{
    unsigned int i;

    for (i = 0; i < count; i++)
    {
        if (wolfPlotter_setPin(pin, 1) != 0)
            return -1;

        usleep(delay_ms * 1000);

        if (wolfPlotter_setPin(pin, 0) != 0)
            return -1;

        usleep(delay_ms * 1000);
    }

    return 0;
}

/* ---- Pen-plotter stubs ---- */

int wolfPlotter_moveX(int steps)
{
    printf("wolfPlotter: moveX(%d) — stub, not yet implemented\n", steps);
    return 0;
}

int wolfPlotter_moveY(int steps)
{
    printf("wolfPlotter: moveY(%d) — stub, not yet implemented\n", steps);
    return 0;
}

int wolfPlotter_pencilDown(void)
{
    printf("wolfPlotter: pencilDown() — stub, not yet implemented\n");
    return 0;
}

int wolfPlotter_pencilUp(void)
{
    printf("wolfPlotter: pencilUp() — stub, not yet implemented\n");
    return 0;
}

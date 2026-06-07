#include <stdio.h>
#include "wolfPlotter.h"

int main(void)
{
    printf("=== wolfPlotter Blink Test ===\n");
    printf("Target: GPIO 19, 5 blink, 500ms on/off\n\n");

    if (wolfPlotter_init() != 0)
    {
        fprintf(stderr, "Failed to initialize wolfPlotter\n");
        return 1;
    }

    wolfPlotter_blink(19, 5, 500);

    wolfPlotter_cleanup();

    printf("\nDone.\n");
    return 0;
}

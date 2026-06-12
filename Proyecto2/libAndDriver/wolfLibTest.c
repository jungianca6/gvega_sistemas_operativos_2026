#include <stdio.h>
#include "wolfPlotter.h"

#define WORD "123"

int main(void)
{
    printf("=== wolfPlotter writeWord Test ===\n");
    printf("Word to write: \"%s\"\n\n", WORD);

    /* Initialize the library (opens /proc/penplttr-gpio) */
    if (wolfPlotter_init() != 0)
    {
        fprintf(stderr, "Failed to initialize wolfPlotter\n");
        return 1;
    }

    /* Write the word */
    if (wolfPlotter_writeWord(WORD) != 0)
    {
        fprintf(stderr, "writeWord failed\n");
        wolfPlotter_cleanup();
        return 1;
    }

    /* Clean up */
    wolfPlotter_cleanup();

    printf("\n=== Test complete ===\n");
    return 0;
}

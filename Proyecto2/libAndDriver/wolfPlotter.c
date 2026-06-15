#include <stdio.h>
#include <string.h>
#include <ctype.h>
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

/* ---- Stepper motor control ---- */

int wolfPlotter_move(char axis, int steps, unsigned int delay_us)
{
    char cmd[PENPLTTR_CMD_BUF_SIZE];

    if (proc_file == NULL)
    {
        fprintf(stderr, "wolfPlotter: not initialized, call wolfPlotter_init() first\n");
        return -1;
    }

    if (axis != 'x' && axis != 'X' && axis != 'y' && axis != 'Y')
    {
        fprintf(stderr, "wolfPlotter: invalid axis '%c' (must be 'x' or 'y')\n", axis);
        return -1;
    }

    if (steps == 0)
        return 0;

    snprintf(cmd, sizeof(cmd), "move,%c,%d,%u\n", axis, steps, delay_us);

    if (fputs(cmd, proc_file) == EOF)
    {
        perror("wolfPlotter: failed to write move command");
        return -1;
    }
    fflush(proc_file);

    printf("wolfPlotter: move(%c, %d steps, %u us delay)\n", axis, steps, delay_us);
    return 0;
}

int wolfPlotter_moveX(int steps)
{
    return wolfPlotter_move('x', steps, 1000);
}

int wolfPlotter_moveY(int steps)
{
    return wolfPlotter_move('y', steps, 1000);
}

/* ---- Pen servo (not yet implemented) ---- */

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

/* ---- Word writing ---- */

/* Letter cell dimensions (stepper motor steps) */
#define XMOV           512    /* horizontal width of one letter segment   */
#define YMOV           512    /* full vertical height of one letter cell  */
#define LETTER_SPACING 512    /* horizontal gap between letters           */
#define X_AXIS_MAX     9216   /* physical X-axis rail limit (steps)       */

/*
 * Position tracking — reset at the start of each writeWord() call.
 *   xMovTotal : cumulative X displacement from the starting position
 *   yLastMov  : cumulative Y offset from writing baseline (0 = bottom)
 */
static int xMovTotal = 0;
static int yLastMov  = 0;

/**
 * Tracked movement helpers.
 * Every motor command inside drawLetter() and writeWord() goes through
 * these so that xMovTotal / yLastMov always reflect the pen position.
 */
static void doMoveX(int steps)
{
    wolfPlotter_moveX(steps);
    xMovTotal += steps;
}

static void doMoveY(int steps)
{
    wolfPlotter_moveY(steps);
    yLastMov += steps;
}

/**
 * Draw a single uppercase letter as a 7-segment display glyph.
 *
 * Coordinate system (pen starts at bottom-left of letter cell):
 *   moveY(-) = UP      moveX(+) = RIGHT
 *   moveY(+) = DOWN    moveX(-) = LEFT
 *
 * 7-segment layout            Key positions
 *      ___  a (top)            TL──────TR   y = -YMOV
 *  f→ |   | ←b                │        │
 *      ___  g (middle)         ML──────MR   y = -YMOV/2
 *  e→ |   | ←c                │        │
 *      ___  d (bottom)         BL──────BR   y = 0
 *
 * Each path starts at BL (0, 0) and ends at BR (XMOV, 0),
 * except W which ends at (2·XMOV, 0) since it draws two UU.
 * Without pen-up / pen-down, some paths retrace segments or
 * draw minor extras to maintain a continuous stroke.
 */
static void drawLetter(char letter)
{
    switch (letter)
    {
    /* ----  A  (a,b,c,e,f,g)  ----
     *  _
     * |_|
     * | |                         */
    case 'A':
        doMoveY(-YMOV);            /* e+f : left side up           */
        doMoveX(XMOV);             /* a   : top bar right          */
        doMoveY(YMOV / 2);         /* b   : right-top down         */
        doMoveX(-XMOV);            /* g   : middle bar left        */
        doMoveX(XMOV);             /* g   : retrace middle right   */
        doMoveY(YMOV / 2);         /* c   : right-bottom down      */
        break;

    /* ----  B  (c,d,e,f,g)  ----
     * |_
     * |_|                         */
    case 'B':
        doMoveY(-YMOV);            /* e+f : left side up           */
        doMoveY(YMOV / 2);         /* retrace f down to middle     */
        doMoveX(XMOV);             /* g   : middle bar right       */
        doMoveY(YMOV / 2);         /* c   : right-bottom down      */
        doMoveX(-XMOV);            /* d   : bottom bar left        */
        doMoveX(XMOV);             /* d   : retrace bottom right   */
        break;

    /* ----  C  (a,d,e,f)  ----
     *  _
     * |
     * |_                          */
    case 'C':
        doMoveY(-YMOV);            /* e+f : left side up           */
        doMoveX(XMOV);             /* a   : top bar right          */
        doMoveX(-XMOV);            /* retrace a back to TL         */
        doMoveY(YMOV);             /* retrace e+f down to BL       */
        doMoveX(XMOV);             /* d   : bottom bar right       */
        break;

    /* ----  D  (b,c,d,e,g)  ----
     *   _|
     * |_|                         */
    case 'D':
        doMoveY(-YMOV / 2);        /* e   : left-bottom up         */
        doMoveX(XMOV);             /* g   : middle bar right       */
        doMoveY(-YMOV / 2);        /* b reverse : up to TR         */
        doMoveY(YMOV);             /* b+c: right side down         */
        doMoveX(-XMOV);            /* d   : bottom bar left        */
        doMoveX(XMOV);             /* d   : retrace bottom right   */
        break;

    /* ----  E  (a,d,e,f,g)  ----
     *  _
     * |_
     * |_                          */
    case 'E':
        doMoveY(-YMOV);            /* e+f : left side up           */
        doMoveX(XMOV);             /* a   : top bar right          */
        doMoveX(-XMOV);            /* retrace a back to TL         */
        doMoveY(YMOV / 2);         /* retrace f down to ML         */
        doMoveX(XMOV);             /* g   : middle bar right       */
        doMoveX(-XMOV);            /* retrace g back to ML         */
        doMoveY(YMOV / 2);         /* retrace e down to BL         */
        doMoveX(XMOV);             /* d   : bottom bar right       */
        break;

    /* ----  F  (a,e,f,g)  ----
     *  _
     * |_
     * |
     * NOTE: extra d drawn to reach BR (same shape as E w/o pen-up) */
    case 'F':
        doMoveY(-YMOV);            /* e+f : left side up           */
        doMoveX(XMOV);             /* a   : top bar right          */
        doMoveX(-XMOV);            /* retrace a back to TL         */
        doMoveY(YMOV / 2);         /* retrace f down to ML         */
        doMoveX(XMOV);             /* g   : middle bar right       */
        doMoveX(-XMOV);            /* retrace g back to ML         */
        doMoveY(YMOV / 2);         /* retrace e down to BL         */
        doMoveX(XMOV);             /* (d) : to reach BR            */
        break;

    /* ----  G  (a,c,d,e,f)  ----
     *  _
     * |
     * |_|                         */
    case 'G':
        doMoveY(-YMOV);            /* e+f : left side up           */
        doMoveX(XMOV);             /* a   : top bar right          */
        doMoveX(-XMOV);            /* retrace a back to TL         */
        doMoveY(YMOV);             /* retrace e+f down to BL       */
        doMoveX(XMOV);             /* d   : bottom bar right       */
        doMoveY(-YMOV / 2);        /* c reverse : up to MR         */
        doMoveY(YMOV / 2);         /* retrace c down to BR         */
        break;

    /* ----  H  (b,c,e,f,g)  ----
     * |_|
     * | |                         */
    case 'H':
        doMoveY(-YMOV);            /* e+f : left side up           */
        doMoveY(YMOV / 2);         /* retrace f down to ML         */
        doMoveX(XMOV);             /* g   : middle bar right       */
        doMoveY(-YMOV / 2);        /* b reverse : up to TR         */
        doMoveY(YMOV);             /* b+c: right side down         */
        break;

    /* ----  I  (e,f)  ----
     * |
     * |
     * NOTE: extra d drawn to reach BR                              */
    case 'I':
        doMoveY(-YMOV);            /* e+f : left side up           */
        doMoveY(YMOV);             /* retrace e+f down to BL       */
        doMoveX(XMOV);             /* (d) : to reach BR            */
        break;

    /* ----  J  (b,c,d)  ----
     *   |
     *  _|                         */
    case 'J':
        doMoveX(XMOV);             /* d   : bottom bar right       */
        doMoveY(-YMOV);            /* c+b reverse : right side up  */
        doMoveY(YMOV);             /* retrace b+c down to BR       */
        break;

    /* ----  K  ≈ H  (b,c,e,f,g)  ----
     * |_|
     * | |                         */
    case 'K':
        doMoveY(-YMOV);
        doMoveY(YMOV / 2);
        doMoveX(XMOV);
        doMoveY(-YMOV / 2);
        doMoveY(YMOV);
        break;

    /* ----  L  (d,e,f)  ----
     * |
     * |_                          */
    case 'L':
        doMoveY(-YMOV);            /* e+f : left side up           */
        doMoveY(YMOV);             /* retrace e+f down to BL       */
        doMoveX(XMOV);             /* d   : bottom bar right       */
        break;

    /* ----  M  ≈ (a,b,c,e,f)  ----
     *  _
     * | |
     * | |                         */
    case 'M':
        doMoveY(-YMOV);            /* e+f : left side up           */
        doMoveX(XMOV);             /* a   : top bar right          */
        doMoveY(YMOV);             /* b+c : right side down        */
        break;

    /* ----  N  (c,e,g — lowercase n)  ----
     *  _
     * | |                         */
    case 'N':
        doMoveY(-YMOV / 2);        /* e   : left-bottom up to ML   */
        doMoveX(XMOV);             /* g   : middle bar right       */
        doMoveY(YMOV / 2);         /* c   : right-bottom down      */
        break;

    /* ----  O  (a,b,c,d,e,f)  ----
     *  _
     * | |
     * |_|                         */
    case 'O':
        doMoveY(-YMOV);            /* e+f : left side up           */
        doMoveX(XMOV);             /* a   : top bar right          */
        doMoveY(YMOV);             /* b+c : right side down        */
        doMoveX(-XMOV);            /* d   : bottom bar left        */
        doMoveX(XMOV);             /* d   : retrace bottom right   */
        break;

    /* ----  P  (a,b,e,f,g)  ----
     *  _
     * |_|
     * |
     * NOTE: extra d drawn to reach BR                              */
    case 'P':
        doMoveY(-YMOV);            /* e+f : left side up           */
        doMoveX(XMOV);             /* a   : top bar right          */
        doMoveY(YMOV / 2);         /* b   : right-top down to MR   */
        doMoveX(-XMOV);            /* g   : middle bar left        */
        doMoveY(YMOV / 2);         /* retrace e down to BL         */
        doMoveX(XMOV);             /* (d) : to reach BR            */
        break;

    /* ----  Q  ≈ A  (a,b,c,f,g + extra e)  ----
     *  _
     * |_|
     * | |
     * NOTE: same shape as A; 7-seg Q ≈ A without pen-up           */
    case 'Q':
        doMoveY(-YMOV);
        doMoveX(XMOV);
        doMoveY(YMOV / 2);
        doMoveX(-XMOV);
        doMoveX(XMOV);
        doMoveY(YMOV / 2);
        break;

    /* ----  R  (e,g — lowercase r)  ----
     *  _
     * |
     * NOTE: extra d drawn to reach BR                              */
    case 'R':
        doMoveY(-YMOV / 2);        /* e   : left-bottom up to ML   */
        doMoveX(XMOV);             /* g   : middle bar right       */
        doMoveX(-XMOV);            /* retrace g back to ML         */
        doMoveY(YMOV / 2);         /* retrace e down to BL         */
        doMoveX(XMOV);
                 
        break;

    /* ----  S  (a,c,d,f,g)  ----
     *  _
     *  _|
     * |_
     * NOTE: extra b from return stroke                             */
    case 'S':
        doMoveX(XMOV);             /* d   : bottom bar right       */
        doMoveY(-YMOV / 2);        /* c reverse : up to MR         */
        doMoveX(-XMOV);            /* g   : middle bar left        */
        doMoveY(-YMOV / 2);        /* f   : left-top up to TL      */
        doMoveX(XMOV);             /* a   : top bar right          */
        doMoveY(YMOV);             /* (b)+c : right side down      */
        break;

    /* ----  T  (d,e,f,g — lowercase t)  ----
     * |_
     * |_                          */
    case 'T':
        doMoveY(-YMOV);            /* e+f : left side up           */
        doMoveY(YMOV / 2);         /* retrace f down to ML         */
        doMoveX(XMOV);             /* g   : middle bar right       */
        doMoveX(-XMOV);            /* retrace g back to ML         */
        doMoveY(YMOV / 2);         /* retrace e down to BL         */
        doMoveX(XMOV);             /* d   : bottom bar right       */
        break;

    /* ----  U  (b,c,d,e,f)  ----
     * | |
     * |_|                         */
    case 'U':
        doMoveY(-YMOV);            /* e+f : left side up           */
        doMoveY(YMOV);             /* retrace e+f down to BL       */
        doMoveX(XMOV);             /* d   : bottom bar right       */
        doMoveY(-YMOV);            /* c+b reverse : right side up  */
        doMoveY(YMOV);             /* retrace b+c down to BR       */
        break;

    /* ----  V  ≈ U  (b,c,d,e,f)  ----
     * | |
     * |_|                         */
    case 'V':
        doMoveY(-YMOV);
        doMoveY(YMOV);
        doMoveX(XMOV);
        doMoveY(-YMOV);
        doMoveY(YMOV);
        break;

    /* ----  W  = two UU side by side (double width)  ----
     * | | |
     * |_|_|
     * NOTE: net X = 2·XMOV                                        */
    case 'W':
        /* First U */
        doMoveY(-YMOV);
        doMoveY(YMOV);
        doMoveX(XMOV);
        doMoveY(-YMOV);
        doMoveY(YMOV);
        /* Second U */
        doMoveY(-YMOV);
        doMoveY(YMOV);
        doMoveX(XMOV);
        doMoveY(-YMOV);
        doMoveY(YMOV);
        break;

    /* ----  X  ≈ H  (b,c,e,f,g)  ----
     * |_|
     * | |                         */
    case 'X':
        doMoveY(-YMOV);
        doMoveY(YMOV / 2);
        doMoveX(XMOV);
        doMoveY(-YMOV / 2);
        doMoveY(YMOV);
        break;

    /* ----  Y  (b,c,d,f,g + extra e)  ----
     * |_|
     *  _|                         */
    case 'Y':
        doMoveY(-YMOV);            /* e(extra)+f : left side up    */
        doMoveY(YMOV / 2);         /* retrace f down to ML         */
        doMoveX(XMOV);             /* g   : middle bar right       */
        doMoveY(-YMOV / 2);        /* b reverse : up to TR         */
        doMoveY(YMOV);             /* b+c : right side down        */
        doMoveX(-XMOV);            /* d   : bottom bar left        */
        doMoveX(XMOV);             /* d   : retrace bottom right   */
        break;

    /* ----  Z  (a,b,d,e,g + extra f)  ----
     *  _
     * |_|
     * |_                          */
    case 'Z':
        doMoveY(-YMOV);            /* e+f(extra) : left side up    */
        doMoveX(XMOV);             /* a   : top bar right          */
        doMoveY(YMOV / 2);         /* b   : right-top down to MR   */
        doMoveX(-XMOV);            /* g   : middle bar left        */
        doMoveY(YMOV / 2);         /* retrace e down to BL         */
        doMoveX(XMOV);             /* d   : bottom bar right       */
        break;

    /* ---- Digits ---- */

    /* ----  0  (a,b,c,d,e,f) — same as O  ----
     *  _
     * | |
     * |_|                         */
    case '0':
        doMoveY(-YMOV);            /* e+f : left side up           */
        doMoveX(XMOV);             /* a   : top bar right          */
        doMoveY(YMOV);             /* b+c : right side down        */
        doMoveX(-XMOV);            /* d   : bottom bar left        */
        doMoveX(XMOV);             /* d   : retrace bottom right   */
        break;

    /* ----  1  (b,c)  ----
     *   |
     *   |                         */
    case '1':
        doMoveX(XMOV);             /* move to BR                   */
        doMoveY(-YMOV);            /* c+b reverse : right side up  */
        doMoveY(YMOV);             /* retrace b+c down to BR       */
        break;

    /* ----  2  (a,b,d,e,g)  ----
     *  _
     *  _|
     * |_                          */
    case '2':
        doMoveX(XMOV);             /* d   : bottom bar right       */
        doMoveY(-YMOV / 2);        /* c reverse (drawn as b) up MR */
        doMoveX(-XMOV);            /* g   : middle bar left        */
        doMoveY(YMOV / 2);         /* e   : left-bottom down to BL */
        doMoveY(-YMOV);            /* e+f : left side up (retrace) */
        doMoveX(XMOV);             /* a   : top bar right          */
        doMoveY(YMOV);             /* b+c : right side down to BR  */
        break;

    /* ----  3  (a,b,c,d,g)  ----
     *  _
     *  _|
     *  _|                         */
    case '3':
        doMoveX(XMOV);             /* d   : bottom bar right       */
        doMoveY(-YMOV / 2);        /* c reverse : up to MR         */
        doMoveX(-XMOV);            /* g   : middle bar left        */
        doMoveX(XMOV);             /* g   : retrace middle right   */
        doMoveY(-YMOV / 2);        /* b reverse : up to TR         */
        doMoveX(-XMOV);            /* a   : top bar left           */
        doMoveX(XMOV);             /* a   : retrace top right      */
        doMoveY(YMOV);             /* b+c : right side down to BR  */
        break;

    /* ----  4  (b,c,f,g)  ----
     * |_|
     *   |                         */
    case '4':
        doMoveY(-YMOV);            /* e(extra)+f : left side up    */
        doMoveY(YMOV / 2);         /* retrace f down to ML         */
        doMoveX(XMOV);             /* g   : middle bar right       */
        doMoveY(-YMOV / 2);        /* b reverse : up to TR         */
        doMoveY(YMOV);             /* b+c : right side down to BR  */
        break;

    /* ----  5  (a,c,d,f,g) — same as S  ----
     *  _
     *  _|
     * |_                          */
    case '5':
        doMoveX(XMOV);             /* d   : bottom bar right       */
        doMoveY(-YMOV / 2);        /* c reverse : up to MR         */
        doMoveX(-XMOV);            /* g   : middle bar left        */
        doMoveY(-YMOV / 2);        /* f   : left-top up to TL      */
        doMoveX(XMOV);             /* a   : top bar right          */
        doMoveY(YMOV);             /* (b)+c : right side down      */
        break;

    /* ----  6  (a,c,d,e,f,g)  ----
     *  _
     * |_
     * |_|                         */
    case '6':
        doMoveY(-YMOV);            /* e+f : left side up           */
        doMoveX(XMOV);             /* a   : top bar right          */
        doMoveX(-XMOV);            /* retrace a back to TL         */
        doMoveY(YMOV / 2);         /* retrace f down to ML         */
        doMoveX(XMOV);             /* g   : middle bar right       */
        doMoveY(YMOV / 2);         /* c   : right-bottom down      */
        doMoveX(-XMOV);            /* d   : bottom bar left        */
        doMoveX(XMOV);             /* d   : retrace bottom right   */
        break;

    /* ----  7  (a,b,c)  ----
     *  _
     *   |
     *   |                         */
    case '7':
        doMoveY(-YMOV);            /* e+f(extra) : left side up    */
        doMoveX(XMOV);             /* a   : top bar right          */
        doMoveY(YMOV);             /* b+c : right side down to BR  */
        break;

    /* ----  8  (a,b,c,d,e,f,g) — all segments  ----
     *  _
     * |_|
     * |_|                         */
    case '8':
        doMoveY(-YMOV);            /* e+f : left side up           */
        doMoveX(XMOV);             /* a   : top bar right          */
        doMoveY(YMOV / 2);         /* b   : right-top down to MR   */
        doMoveX(-XMOV);            /* g   : middle bar left        */
        doMoveX(XMOV);             /* g   : retrace middle right   */
        doMoveY(YMOV / 2);         /* c   : right-bottom down      */
        doMoveX(-XMOV);            /* d   : bottom bar left        */
        doMoveX(XMOV);             /* d   : retrace bottom right   */
        break;

    /* ----  9  (a,b,c,d,f,g)  ----
     *  _
     * |_|
     *  _|                         */
    case '9':
        doMoveY(-YMOV);            /* e(extra)+f : left side up    */
        doMoveX(XMOV);             /* a   : top bar right          */
        doMoveY(YMOV / 2);         /* b   : right-top down to MR   */
        doMoveX(-XMOV);            /* g   : middle bar left        */
        doMoveX(XMOV);             /* g   : retrace middle right   */
        doMoveY(YMOV / 2);         /* c   : right-bottom down      */
        doMoveX(-XMOV);            /* d   : bottom bar left        */
        doMoveX(XMOV);             /* d   : retrace bottom right   */
        break;

    default:
        printf("wolfPlotter: '%c' not supported (A-Z, 0-9)\n",
               letter);
        break;
    }
}

int wolfPlotter_writeWord(const char *word)
{
    int i;
    int len;
    char ch;

    if (word == NULL)
    {
        fprintf(stderr, "wolfPlotter: writeWord called with NULL\n");
        return -1;
    }

    len = strlen(word);
    if (len == 0)
    {
        printf("wolfPlotter: empty word, nothing to write\n");
        return 0;
    }

    /* Reset position tracking */
    xMovTotal = 0;
    yLastMov  = 0;

    printf("wolfPlotter: writing word \"%s\" (%d letters)\n", word, len);

    /*
     * Initial positioning: pen starts at top of Y-axis rail,
     * move to the writing baseline (bottom of letter cells).
     * This is NOT tracked in yLastMov (it is a one-time setup).
     */
    //wolfPlotter_moveY(-(2 * YMOV));

    for (i = 0; i < len; i++)
    {
        /* Convert to uppercase */
        ch = toupper((unsigned char)word[i]);

        /* Bounds check: will the next letter fit on the rail? */
        if (xMovTotal + XMOV > X_AXIS_MAX)
        {
            printf("wolfPlotter: X-axis limit (%d steps) reached "
                   "at letter %d ('%c'), stopping\n",
                   X_AXIS_MAX, i + 1, ch);
            break;
        }

        printf("wolfPlotter: drawing letter '%c' (%d/%d)\n",
               ch, i + 1, len);

        /* Draw the letter strokes */
        drawLetter(ch);

        /* Advance carriage for spacing (except after last letter) */
        if (i < len - 1)
        {
    
            doMoveX(LETTER_SPACING);
        }
    }

    /* ---- Carriage return ---- */
    printf("wolfPlotter: carriage return — moveX(%d), Y offset: %d\n",
           -xMovTotal, yLastMov);

    /* Return X to starting position */
    if (xMovTotal != 0)
        wolfPlotter_moveX(-xMovTotal);

    /* Correct Y if any letter left the pen above baseline */
    if (yLastMov != 0)
        wolfPlotter_moveY(-yLastMov);

    printf("wolfPlotter: finished writing \"%s\"\n", word);
    return 0;
}

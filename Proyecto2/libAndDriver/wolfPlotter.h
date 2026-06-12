#ifndef WOLF_PLOTTER_H
#define WOLF_PLOTTER_H

/**
 * wolfPlotter - Userspace library for the penplttr-gpio driver.
 *
 * Communicates with the kernel driver through /proc/penplttr-gpio.
 * Provides GPIO control primitives and pen-plotter motor functions.
 */

/**
 * Initialize the wolfPlotter library.
 * Opens the /proc/penplttr-gpio device file.
 *
 * @return 0 on success, -1 on failure.
 */
int wolfPlotter_init(void);

/**
 * Release all resources held by the library.
 */
void wolfPlotter_cleanup(void);

/**
 * Set a GPIO pin to a given value.
 *
 * @param pin   GPIO pin number (0-27).
 * @param value 1 for HIGH, 0 for LOW.
 * @return 0 on success, -1 on failure.
 */
int wolfPlotter_setPin(unsigned int pin, unsigned int value);

/**
 * Blink an LED connected to the given GPIO pin.
 *
 * @param pin      GPIO pin number (0-27).
 * @param count    Number of blink cycles.
 * @param delay_ms Milliseconds for each on/off phase.
 * @return 0 on success, -1 on failure.
 */
int wolfPlotter_blink(unsigned int pin, unsigned int count, unsigned int delay_ms);

/* ---- Stepper motor control ---- */

/**
 * Move a stepper motor by the given number of steps.
 * This is the low-level function used by wolfPlotter_moveX/moveY.
 *
 * @param axis     'x' or 'y' to select which motor.
 * @param steps    Number of steps (negative = reverse direction).
 * @param delay_us Microseconds between each step (1000 = 1ms).
 * @return 0 on success, -1 on failure.
 */
int wolfPlotter_move(char axis, int steps, unsigned int delay_us);

/**
 * Move the X-axis stepper motor by the given number of steps.
 * Uses 1ms step delay (matching stepper.py default).
 *
 * @param steps Positive = counter-clockwise, negative = clockwise.
 * @return 0 on success, -1 on failure.
 */
int wolfPlotter_moveX(int steps);

/**
 * Move the Y-axis stepper motor by the given number of steps.
 * Uses 1ms step delay (matching stepper.py default).
 *
 * @param steps Positive = counter-clockwise, negative = clockwise.
 * @return 0 on success, -1 on failure.
 */
int wolfPlotter_moveY(int steps);

/**
 * Lower the pencil onto the drawing surface.
 *
 * @return 0 on success, -1 on failure.
 */
int wolfPlotter_pencilDown(void);

/**
 * Raise the pencil off the drawing surface.
 *
 * @return 0 on success, -1 on failure.
 */
int wolfPlotter_pencilUp(void);

/* ---- Word writing ---- */

/**
 * Write a word by drawing each uppercase letter with the pen plotter.
 *
 * Each letter occupies a cell of XMOV (512) × YMOV (512) steps.
 * Maximum X-axis travel: 9216 steps; words exceeding this are truncated.
 * After the last letter the carriage returns to the starting X position
 * and the Y-axis is corrected to the writing baseline.
 * Lowercase letters are treated as their uppercase equivalents.
 * Non-alphabetic characters are skipped with a warning.
 *
 * @param word  Null-terminated string to write.
 * @return 0 on success, -1 on failure.
 */
int wolfPlotter_writeWord(const char *word);

#endif /* WOLF_PLOTTER_H */

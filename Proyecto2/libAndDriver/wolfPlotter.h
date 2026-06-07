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

/* ---- Pen-plotter motor functions (stubs for future implementation) ---- */

/**
 * Move the X-axis motor by the given number of steps.
 *
 * @param steps Positive = right, negative = left.
 * @return 0 on success, -1 on failure.
 */
int wolfPlotter_moveX(int steps);

/**
 * Move the Y-axis motor by the given number of steps.
 *
 * @param steps Positive = forward, negative = backward.
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

#endif /* WOLF_PLOTTER_H */

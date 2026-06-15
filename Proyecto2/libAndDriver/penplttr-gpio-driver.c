#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>

#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/string.h>

#define PENPLTTR_MAX_USER_SIZE 1024
#define PENPLTTR_STATUS_BUF_SIZE 2048
#define PENPLTTR_MAX_PINS 28

/* GPIO offset for BCM2835 on newer Raspberry Pi OS kernels */
#define GPIO_OFFSET 512

/* ---- Stepper motor definitions ---- */
#define STEPPER_NUM_PHASES 8
#define STEPPER_PINS_PER_MOTOR 4
#define STEPPER_DEFAULT_DELAY_US 1000  /* 1 ms, matching stepper.py */

/*
 * Half-step sequence for 28BYJ-48 via ULN2003.
 * Identical to stepper.py's step_sequence.
 * Each row = [in1, in2, in3, in4].
 */
static const int step_sequence[STEPPER_NUM_PHASES][STEPPER_PINS_PER_MOTOR] = {
	{1, 0, 0, 1},
	{1, 0, 0, 0},
	{1, 1, 0, 0},
	{0, 1, 0, 0},
	{0, 1, 1, 0},
	{0, 0, 1, 0},
	{0, 0, 1, 1},
	{0, 0, 0, 1}
};

/* Motor X: BCM 14, 15, 18, 23 */
static const unsigned int motor_x_pins[STEPPER_PINS_PER_MOTOR] = {14, 15, 18, 23};

/* Motor Y: BCM 17, 4, 27, 22 */
static const unsigned int motor_y_pins[STEPPER_PINS_PER_MOTOR] = {17, 4, 27, 22};

/* Per-motor phase counter (tracks position in the 8-step sequence) */
static int motor_x_phase = 0;
static int motor_y_phase = 0;

/* ---- Per-pin tracking ---- */
static struct gpio_desc *pin_descs[PENPLTTR_MAX_PINS] = {NULL};
static unsigned int  pin_states[PENPLTTR_MAX_PINS] = {0};
static unsigned long pin_writes[PENPLTTR_MAX_PINS] = {0};

/* ---- Global state ---- */
static unsigned long op_count = 0;
static unsigned long load_jiffies = 0;
static struct proc_dir_entry *penplttr_proc = NULL;
static char data_buffer[PENPLTTR_MAX_USER_SIZE + 1] = {0};

/**
 * Request a GPIO pin using the consumer API.
 * Translates BCM pin number to Linux GPIO number (BCM + GPIO_OFFSET).
 */
static int penplttr_request_pin(unsigned int pin)
{
	struct gpio_desc *desc;
	int linux_gpio;
	int ret;

	/* Already requested */
	if (pin_descs[pin])
		return 0;

	/* Translate BCM pin → Linux GPIO number */
	linux_gpio = pin + GPIO_OFFSET;

	/* Get the GPIO descriptor */
	desc = gpio_to_desc(linux_gpio);
	if (!desc)
	{
		printk("penplttr: gpio_to_desc(BCM %d, linux %d) returned NULL\n",
			pin, linux_gpio);
		return -EINVAL;
	}

	/* Configure as output, initially LOW */
	ret = gpiod_direction_output(desc, 0);
	if (ret)
	{
		printk("penplttr: gpiod_direction_output(BCM %d) failed: %d\n", pin, ret);
		return ret;
	}

	pin_descs[pin] = desc;
	printk("penplttr: GPIO BCM %d (linux %d) configured as output\n",
		pin, linux_gpio);
	return 0;
}

/**
 * Release all GPIO pins — set them LOW on unload.
 */
static void penplttr_release_all_pins(void)
{
	int i;

	for (i = 0; i < PENPLTTR_MAX_PINS; i++)
	{
		if (pin_descs[i])
		{
			gpiod_set_value(pin_descs[i], 0);
			pin_descs[i] = NULL;
			printk("penplttr: GPIO BCM %d set LOW and released\n", i);
		}
	}
}

/**
 * Move a stepper motor by the given number of steps.
 *
 * @param pins      Array of 4 BCM pin numbers for this motor.
 * @param phase     Pointer to the motor's phase counter (0-7).
 * @param steps     Number of steps (negative = reverse direction).
 * @param delay_us  Microseconds to sleep between each step.
 * @return 0 on success, negative on error.
 *
 * This replicates the stepper.py logic:
 *   - direction True  → decrement phase (clockwise)
 *   - direction False → increment phase (counter-clockwise)
 * Positive steps = increment (CCW), negative steps = decrement (CW).
 */
static int penplttr_stepper_move(const unsigned int *pins, int *phase,
				 int steps, unsigned int delay_us)
{
	int i, p, ret;
	int direction;   /* +1 or -1 */
	int num_steps;

	if (steps == 0)
		return 0;

	if (steps > 0) {
		direction = 1;   /* increment phase → CCW (stepper.py default) */
		num_steps = steps;
	} else {
		direction = -1;  /* decrement phase → CW */
		num_steps = -steps;
	}

	/* Request all 4 motor pins on first use */
	for (p = 0; p < STEPPER_PINS_PER_MOTOR; p++) {
		ret = penplttr_request_pin(pins[p]);
		if (ret) {
			printk("penplttr: stepper failed to acquire BCM pin %u\n",
			       pins[p]);
			return ret;
		}
	}

	printk("penplttr: stepper move %d steps (dir=%d, delay=%u us) on pins [%u,%u,%u,%u]\n",
	       steps, direction, delay_us, pins[0], pins[1], pins[2], pins[3]);

	/* Step loop — identical logic to stepper.py */
	for (i = 0; i < num_steps; i++) {
		/* Apply the current phase to all 4 pins */
		for (p = 0; p < STEPPER_PINS_PER_MOTOR; p++) {
			gpiod_set_value(pin_descs[pins[p]],
					step_sequence[*phase][p]);

			/* Update per-pin tracking */
			pin_states[pins[p]] = step_sequence[*phase][p];
			pin_writes[pins[p]]++;
		}
		op_count++;

		/* Advance phase in the chosen direction (wrap 0-7) */
		*phase = (*phase + direction + STEPPER_NUM_PHASES) % STEPPER_NUM_PHASES;

		/* Sleep between steps */
		usleep_range(delay_us, delay_us + 100);
	}

	/* Cleanup: set all motor pins LOW (same as stepper.py cleanup) */
	for (p = 0; p < STEPPER_PINS_PER_MOTOR; p++) {
		gpiod_set_value(pin_descs[pins[p]], 0);
		pin_states[pins[p]] = 0;
	}

	printk("penplttr: stepper move complete\n");
	return 0;
}


static ssize_t penplttr_read(struct file *file, char __user *user, size_t size, loff_t *off)
{
	char buf[PENPLTTR_STATUS_BUF_SIZE];
	int len = 0;
	int i;
	unsigned long uptime_secs;

	if (*off > 0)
		return 0;

	uptime_secs = (jiffies - load_jiffies) / HZ;

	len += snprintf(buf + len, sizeof(buf) - len,
		"=== PenPlttr GPIO Driver v2.2 ===\n"
		"Status: ACTIVE\n"
		"Backend: gpio/consumer (via pinctrl-bcm2835)\n"
		"GPIO offset: %d\n"
		"Uptime: %lu seconds\n"
		"Total operations: %lu\n\n",
		GPIO_OFFSET, uptime_secs, op_count);

	len += snprintf(buf + len, sizeof(buf) - len,
		"BCM Pin | State | Writes | Linux GPIO\n"
		"--------|-------|--------|----------\n");

	for (i = 0; i < PENPLTTR_MAX_PINS; i++)
	{
		if (pin_writes[i] > 0)
		{
			len += snprintf(buf + len, sizeof(buf) - len,
				"     %2d |   %s |   %4lu |       %d\n",
				i,
				pin_states[i] ? "ON " : "OFF",
				pin_writes[i],
				i + GPIO_OFFSET);
		}
	}

	if (op_count == 0)
		len += snprintf(buf + len, sizeof(buf) - len,
			"  (no pins accessed yet)\n");

	len += snprintf(buf + len, sizeof(buf) - len, "\n");

	if (len > size)
		len = size;

	if (copy_to_user(user, buf, len))
		return -EFAULT;

	*off += len;
	return len;
}

static ssize_t penplttr_write(struct file *file, const char __user *user, size_t size, loff_t *off)
{
	unsigned int pin = UINT_MAX;
	unsigned int value = UINT_MAX;
	int ret;

	memset(data_buffer, 0x0, sizeof(data_buffer));

	if (size > PENPLTTR_MAX_USER_SIZE)
	{
		size = PENPLTTR_MAX_USER_SIZE;
	}

	if (copy_from_user(data_buffer, user, size))
		return 0;

	printk("penplttr: data buffer: %s\n", data_buffer);

	/* ---- Handle "move,<axis>,<steps>,<delay_us>" command ---- */
	if (strncmp(data_buffer, "move,", 5) == 0)
	{
		char axis = 0;
		int steps = 0;
		unsigned int delay_us = STEPPER_DEFAULT_DELAY_US;

		/* Parse: move,<axis>,<steps>,<delay_us> */
		if (sscanf(data_buffer, "move,%c,%d,%u", &axis, &steps, &delay_us) < 2)
		{
			printk("penplttr: move command format: move,<x|y>,<steps>[,<delay_us>]\n");
			return size;
		}

		if (axis == 'x' || axis == 'X')
		{
			ret = penplttr_stepper_move(motor_x_pins, &motor_x_phase,
						    steps, delay_us);
		}
		else if (axis == 'y' || axis == 'Y')
		{
			ret = penplttr_stepper_move(motor_y_pins, &motor_y_phase,
						    steps, delay_us);
		}
		else
		{
			printk("penplttr: invalid axis '%c' (must be 'x' or 'y')\n", axis);
			return size;
		}

		if (ret)
			printk("penplttr: stepper move failed: %d\n", ret);

		return size;
	}

	/* ---- Handle existing "pin,value" command ---- */
	if (sscanf(data_buffer, "%d,%d", &pin, &value) != 2)
	{
		printk("penplttr: improper data format submitted\n");
		return size;
	}

	if (pin >= PENPLTTR_MAX_PINS)
	{
		printk("penplttr: invalid pin number %d (valid range: 0-%d)\n",
			pin, PENPLTTR_MAX_PINS - 1);
		return size;
	}

	if (value != 0 && value != 1)
	{
		printk("penplttr: invalid value %d (must be 0 or 1)\n", value);
		return size;
	}

	/* Request pin on first use */
	ret = penplttr_request_pin(pin);
	if (ret)
	{
		printk("penplttr: could not acquire GPIO BCM %d: error %d\n", pin, ret);
		return size;
	}

	/* Set the pin value using the GPIO consumer API */
	printk("penplttr: setting BCM pin %d to %d (via gpiod_set_value)\n", pin, value);
	gpiod_set_value(pin_descs[pin], value);

	/* Track the operation */
	op_count++;
	pin_states[pin] = value;
	pin_writes[pin]++;

	return size;
}

static const struct proc_ops penplttr_proc_fops =
{
	.proc_read = penplttr_read,
	.proc_write = penplttr_write,
};

static int __init penplttr_gpio_driver_init(void)
{
	printk("penplttr: GPIO driver v2.2 loading (gpio/consumer, offset=%d)\n",
		GPIO_OFFSET);

	/* Create the proc-fs entry */
	penplttr_proc = proc_create("penplttr-gpio", 0666, NULL, &penplttr_proc_fops);
	if (penplttr_proc == NULL)
	{
		printk("penplttr: failed to create /proc/penplttr-gpio\n");
		return -1;
	}

	load_jiffies = jiffies;

	printk("penplttr: /proc/penplttr-gpio created\n");
	return 0;
}

static void __exit penplttr_gpio_driver_exit(void)
{
	printk("penplttr: GPIO driver unloading\n");
	penplttr_release_all_pins();
	proc_remove(penplttr_proc);
	printk("penplttr: all resources released\n");
	return;
}

module_init(penplttr_gpio_driver_init);
module_exit(penplttr_gpio_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PenPlttr");
MODULE_DESCRIPTION("GPIO driver for Raspberry Pi 3B+ pen plotter (gpio/consumer)");
MODULE_VERSION("2.2");

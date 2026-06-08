#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>

#include <linux/gpio/consumer.h>

#define PENPLTTR_MAX_USER_SIZE 1024
#define PENPLTTR_STATUS_BUF_SIZE 2048
#define PENPLTTR_MAX_PINS 28

/* GPIO offset for BCM2835 on newer Raspberry Pi OS kernels */
#define GPIO_OFFSET 512

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

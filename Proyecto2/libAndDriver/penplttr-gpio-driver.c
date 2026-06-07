#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>

#include <asm/io.h>

#define PENPLTTR_MAX_USER_SIZE 1024
#define PENPLTTR_STATUS_BUF_SIZE 2048

#define BCM2837_GPIO_ADDRESS 0x3F200000

/* ---- Operation tracking ---- */
static unsigned long op_count = 0;           /* Total write operations */
static unsigned int  pin_states[28] = {0};   /* Last known state per pin */
static unsigned long pin_writes[28] = {0};   /* Write count per pin */
static unsigned long load_jiffies = 0;       /* When the driver was loaded */

static struct proc_dir_entry *penplttr_proc = NULL;

static char data_buffer[PENPLTTR_MAX_USER_SIZE + 1] = {0};

static unsigned int *gpio_registers = NULL;

static void gpio_pin_on(unsigned int pin)
{
	unsigned int fsel_index = pin / 10;
	unsigned int fsel_bitpos = pin % 10;
	unsigned int *gpio_fsel = gpio_registers + fsel_index;
	unsigned int *gpio_on_register = (unsigned int *)((char *)gpio_registers + 0x1c);

	*gpio_fsel &= ~(7 << (fsel_bitpos * 3));
	*gpio_fsel |= (1 << (fsel_bitpos * 3));
	*gpio_on_register |= (1 << pin);

	return;
}

static void gpio_pin_off(unsigned int pin)
{
	unsigned int *gpio_off_register = (unsigned int *)((char *)gpio_registers + 0x28);
	*gpio_off_register |= (1 << pin);
	return;
}

static ssize_t penplttr_read(struct file *file, char __user *user, size_t size, loff_t *off)
{
	char buf[PENPLTTR_STATUS_BUF_SIZE];
	int len = 0;
	int i;
	unsigned long uptime_secs;

	/* Only produce output on first read (offset 0) */
	if (*off > 0)
		return 0;

	uptime_secs = (jiffies - load_jiffies) / HZ;

	len += snprintf(buf + len, sizeof(buf) - len,
		"=== PenPlttr GPIO Driver v1.0 ===\n"
		"Status: ACTIVE\n"
		"Uptime: %lu seconds\n"
		"Total operations: %lu\n"
		"GPIO base: 0x%08X (ioremap)\n\n",
		uptime_secs, op_count, BCM2837_GPIO_ADDRESS);

	len += snprintf(buf + len, sizeof(buf) - len,
		"Pin  | State | Writes\n"
		"-----|-------|-------\n");

	for (i = 0; i < 28; i++)
	{
		if (pin_writes[i] > 0)
		{
			len += snprintf(buf + len, sizeof(buf) - len,
				"  %2d |   %s |   %lu\n",
				i,
				pin_states[i] ? "ON " : "OFF",
				pin_writes[i]);
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

	if (pin > 27)
	{
		printk("penplttr: invalid pin number %d (valid range: 0-27)\n", pin);
		return size;
	}

	if (value != 0 && value != 1)
	{
		printk("penplttr: invalid value %d (must be 0 or 1)\n", value);
		return size;
	}

	printk("penplttr: setting pin %d to %d\n", pin, value);
	if (value == 1)
	{
		gpio_pin_on(pin);
	}
	else if (value == 0)
	{
		gpio_pin_off(pin);
	}

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
	printk("penplttr: GPIO driver loading\n");

	gpio_registers = (int *)ioremap(BCM2837_GPIO_ADDRESS, PAGE_SIZE);
	if (gpio_registers == NULL)
	{
		printk("penplttr: failed to map GPIO memory\n");
		return -1;
	}

	printk("penplttr: successfully mapped GPIO memory\n");

	/* Create an entry in the proc-fs */
	penplttr_proc = proc_create("penplttr-gpio", 0666, NULL, &penplttr_proc_fops);
	if (penplttr_proc == NULL)
	{
		iounmap(gpio_registers);
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
	iounmap(gpio_registers);
	proc_remove(penplttr_proc);
	return;
}

module_init(penplttr_gpio_driver_init);
module_exit(penplttr_gpio_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PenPlttr");
MODULE_DESCRIPTION("GPIO driver for Raspberry Pi 3B+ pen plotter");
MODULE_VERSION("1.0");

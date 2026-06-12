# -----------------------------------------------------------
#  PenPlttr GPIO Driver & wolfPlotter Library — Makefile
# -----------------------------------------------------------

# Kernel module object
obj-m += penplttr-gpio-driver.o

KDIR   = /lib/modules/$(shell uname -r)/build
PWD    = $(shell pwd)

# Userspace compiler settings
CC     = gcc
CFLAGS = -Wall -Wextra -O2
AR     = ar

# ---- Default: build everything ----
all: driver lib

# ---- Kernel module ----
driver:
	make -C $(KDIR) M=$(PWD) modules

# ---- wolfPlotter static library ----
wolfPlotter.o: wolfPlotter.c wolfPlotter.h
	$(CC) $(CFLAGS) -c wolfPlotter.c -o wolfPlotter.o

libwolfPlotter.a: wolfPlotter.o
	$(AR) rcs libwolfPlotter.a wolfPlotter.o

lib: libwolfPlotter.a

# ---- Blink test binary ----
blink_test: blink_test.c libwolfPlotter.a
	$(CC) $(CFLAGS) blink_test.c -L. -lwolfPlotter -o blink_test

# ---- writeWord test binary ----
wolfLibTest: wolfLibTest.c libwolfPlotter.a
	$(CC) $(CFLAGS) wolfLibTest.c -L. -lwolfPlotter -o wolfLibTest

test: blink_test wolfLibTest

# ---- Load: insert our driver ----
# Our driver uses gpio/consumer API — it works WITH pinctrl-bcm2835,
# requesting pins through the kernel GPIO framework. No unbind needed.
load:
	@echo ">>> Inserting penplttr-gpio-driver.ko..."
	sudo insmod penplttr-gpio-driver.ko
	@echo ">>> Driver loaded. /proc/penplttr-gpio is ready."

# ---- Unload: remove our driver ----
# All GPIO pins are released back to the kernel automatically.
unload:
	@echo ">>> Removing penplttr-gpio-driver..."
	sudo rmmod penplttr-gpio-driver
	@echo ">>> Driver removed. All GPIO pins released."

# ---- Run tests ----
run:
	./blink_test

run_word:
	./wolfLibTest

# ---- Clean everything ----
clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f wolfPlotter.o libwolfPlotter.a blink_test wolfLibTest

# ---- Verify: prove our driver is in control ----
verify:
	@echo "========================================"
	@echo "  PenPlttr Driver Verification Report"
	@echo "========================================"
	@echo ""
	@echo "--- 1. Module loaded? ---"
	@lsmod | grep penplttr || echo "  NOT LOADED"
	@echo ""
	@echo "--- 2. /proc entry exists? ---"
	@ls -la /proc/penplttr-gpio 2>/dev/null || echo "  NOT FOUND"
	@echo ""
	@echo "--- 3. Driver status & operation audit ---"
	@cat /proc/penplttr-gpio 2>/dev/null || echo "  (driver not loaded)"
	@echo "--- 4. Kernel log (last 15 penplttr entries) ---"
	@sudo dmesg | grep penplttr | tail -15
	@echo ""
	@echo "========================================"

.PHONY: all driver lib test load unload clean verify run run_word


TARGET  := bootloader.efi 
DISK_IMG := uefi_disk.img 

CC := clang

CFLAGS := \
	-target x86_64-unknown-windows  \
	-fno-stack-protector			\
	-fno-stack-check				\
	-fshort-wchar					\
	-mno-red-zone					\
	-ffreestanding					\
	-Wall -Wextra					\
	-std=c11  						\
	
#std=c11 asumo que es la version de C usada

# lld-link usa sintaxis /flag:value

LDFLAGS := \
	-target x86_64-unknown-windows	\
	-nostdlib						\
	-fuse-ld=lld					\
	-Wl,/subsystem:efi_application	\
	-Wl,/entry:efi_main				\
	-Wl,/dll						\

.PHONY: all clean disk run

all: $(TARGET)

main.o: main.c efitypes.h game.h
	$(CC) $(CFLAGS) -c $< -o $@

game.o: game.c efitypes.h game.h
	$(CC) $(CFLAGS) -c $< -o $@

OBJS := main.o game.o

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@
	@echo ""
	@echo "Built: $(TARGET)"
	@file $(TARGET)

disk: $(TARGET)
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=64
	mkfs.vfat -F 32 $(DISK_IMG)
	mkdir -p  /tmp/efi_mnt
	sudo mount $(DISK_IMG) /tmp/efi_mnt
	sudo mkdir -p /tmp/efi_mnt/EFI/BOOT 
	sudo cp $(TARGET) /tmp/efi_mnt/EFI/BOOT/BOOTX64.EFI 
	sudo umount /tmp/efi_mnt
	@echo "Imagen lista: $(DISK_IMG)"


run: disk
	qemu-system-x86_64 \
		-bios /usr/share/ovmf/OVMF.fd \
		-drive format=raw,file=$(DISK_IMG)\
		-net none

clean:
	rm -f *.o *.efi $(DISK_IMG)


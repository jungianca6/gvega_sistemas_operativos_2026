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

#hay 8 lineas entre estos 2
#no se si hay algo más ahí

$(TARGET): main.o
	$(CC) $(LDFLAGS)  $< -o $@
	@echo ""
	@echo "Built: $(TARGET)"
	@file $(TARGET)

disk: $(TARGET)
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count
	mkfs.vfat -F 32 $(DISK_IMG)
	mkdir -p  /tmp/efi_mnt
	sudo mount $(DISK_IMG) /tmp/efi_mnt
	sudo mkdir -p /tmp/efi_mnt/EFI/BOOT 
	sudo cp $(TARGET) /tmp/efi_mnt/EFI/BOOT/ 
	sudo umount /tmp/efi_mnt
	@echo "Imagen lista: $(DISK_IMG)"

#despues de EFI/BOOT/ habia algo que empezaba con BO pero no se logra ver en la captura

run: disk
	qemu-system-x86_64 \
		-bios /usr/share/ovmf/OVMF.fd \
		-drive format=raw, file=$(DISK_IMG)\
		-net none

clean:
	rm -f *.o *.efi $(DISK_IMG)


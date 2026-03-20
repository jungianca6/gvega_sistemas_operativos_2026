build:
	nasm -f bin stage2.asm -o stage2.bin
	nasm -f bin os.asm -o os.bin

emulate:
	qemu-system-x86_64 -drive format=raw,file=os.bin

clean:
	rm stage2.bin
	rm os.bin

write:
	sudo dd if=./os.bin of=/dev/sdb
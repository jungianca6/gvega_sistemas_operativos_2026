all:
		rm -rf build
		mkdir build
		nasm -f bin src/FindingTreasure.asm -o build/FindingTreasure.bin
		nasm -f bin src/boot.asm -o build/boot.bin
		cat build/boot.bin build/FindingTreasure.bin > build/game.bin

		qemu-system-x86_64 -fda build/game.bin 

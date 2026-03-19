org   0x7c00    ;indica la posicion de memoria donde estara el codigo, ain final establece
%define SECTOR_AMOUNT 0x4   ;Precompiler defined value for easy changing  1sector 512bytes 
jmp short start 

start:
;inicializa registros
cli ;deshabilita interrupciones globales
xor ax, ax
mov ds, ax
mov ss, ax 
mov es, ax
mov fs, ax
mov gs, ax
mov sp, 0x6ef0 ;Se coloca stack pointer a una posicion, ej:0x6ef0
sti ;habilita interrupciones

mov ah, 0           ;Se resetea el modo del disco
int 0x13            ;interrupcion para utilizar el disco con el bios
                    ;Read from harddrive and write to RAM

mov bx, 0x8000          ;bx= direccion de memoria para leer (puntero)   
mov al, SECTOR_AMOUNT   ; al = cantidad de sectores para lectura
mov ch, 0       ;cilindro = 0
mov dh, 0       ;cabeza =0
mov cl, 2       ;sector = 2
mov ah, 2       ;ah=2 lee desde el disco
int 0x13        ;interrupcion para utilizar el disco con el bios
jmp 0x8000

;   PADDING AND SIGNATURE
times 510-($-$$) db 0; rellena 512 con cero
db 0x55
db 0xaa; numero magico, es para indicar que es bootloader
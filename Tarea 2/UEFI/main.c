#include "game.h"
#include "efitypes.h"

/* ========================= /
/ FUNCIONES AUXILIARES /
/ ========================= */

/*
 * print()
 *
 * Función auxiliar para imprimir una cadena en pantalla
 * usando el protocolo de salida de texto de UEFI.
 *
 * UEFI utiliza UTF-16 (UINT16) para las cadenas de texto.
 * El protocolo ConOut proporciona OutputString() para
 * escribir directamente en la consola del firmware.
 */

void print(EFI_SYSTEM_TABLE *SystemTable, UINT16 *str){
    SystemTable->ConOut->OutputString(SystemTable->ConOut, str);
}

/*
 * get_key()
 *
 * Espera de forma bloqueante hasta que el usuario presione
 * una tecla en el teclado.
 *
 * Utiliza el protocolo EFI_SIMPLE_TEXT_INPUT_PROTOCOL
 * proporcionado por UEFI para leer eventos del teclado.
 *
 * ReadKeyStroke retorna EFI_SUCCESS cuando existe una tecla
 * disponible en el buffer del firmware.
 */


EFI_INPUT_KEY get_key(EFI_SYSTEM_TABLE *SystemTable){
    EFI_INPUT_KEY key;
    while (SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key) != EFI_SUCCESS);
    return key;
}
/* ========================= /
/ EFI ENTRY POINT /
/ ========================= */

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable){
    (void)ImageHandle;

start:
    /* limpiar la pantalla de la consola UEFI */
    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);

    /* mostrar menú principal */
    print(SystemTable, (UINT16*)L"Hola\r\n");
    print(SystemTable, (UINT16*)L"Presione cualquier tecla para empezar... \r\n");
    print(SystemTable, (UINT16*)L"Presione la tecla R para reiniciar... \r\n");
    print(SystemTable, (UINT16*)L"Presione la tecla S para salir... \r\n");

    /* leer una tecla del usuario */
    EFI_INPUT_KEY key = get_key(SystemTable);

    /* salir al firmware/BIOS */
    if(key.UnicodeChar == 's' || key.UnicodeChar == 'S')
        return EFI_SUCCESS;

    /* reiniciar menú */
    if(key.UnicodeChar == 'r' || key.UnicodeChar == 'R')
        goto start;

    /* iniciar el juego */
    start_game(SystemTable);
   
    /* al terminar el juego regresar al menú */
    goto start;

    //get_key(SystemTable);
}
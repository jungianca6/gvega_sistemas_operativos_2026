#include "game.h"
#include "efitypes.h"

/* ========================= /
/ FUNCIONES AUXILIARES /
/ ========================= */

void print(EFI_SYSTEM_TABLE *SystemTable, UINT16 *str){
    SystemTable->ConOut->OutputString(SystemTable->ConOut, str);
}

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

    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);

    print(SystemTable, (UINT16*)L"Hola\r\n");
    print(SystemTable, (UINT16*)L"Presione cualquier tecla para empezar... \r\n");
    print(SystemTable, (UINT16*)L"Presione la tecla R para reiniciar... \r\n");
    print(SystemTable, (UINT16*)L"Presione la tecla S para salir... \r\n");

    EFI_INPUT_KEY key = get_key(SystemTable);

    if(key.UnicodeChar == 's' || key.UnicodeChar == 'S')
        return EFI_SUCCESS;

    if(key.UnicodeChar == 'r' || key.UnicodeChar == 'R')
        goto start;

    start_game(SystemTable);

    // get_key(SystemTable);

    goto start;
}
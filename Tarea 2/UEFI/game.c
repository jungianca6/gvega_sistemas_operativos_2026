#include "game.h"

void start_game(EFI_SYSTEM_TABLE *SystemTable){

    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    SystemTable->ConOut->OutputString(SystemTable->ConOut, (UINT16*)L"GIANCARLO\r\n");
}
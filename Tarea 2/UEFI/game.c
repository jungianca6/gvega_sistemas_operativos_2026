#include "game.h"

static unsigned int seed = 1234567;

unsigned int rand_simple(){
    seed = seed * 1103515245 + 12345;
    return seed;
}

void start_game(EFI_SYSTEM_TABLE *SystemTable){

    UINTN cols;
    UINTN rows;

    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);

    /* usar el reloj como seed */
    EFI_TIME time;
    SystemTable->RuntimeServices->GetTime(&time, NULL);

    seed = time.Nanosecond + time.Second;

    /* obtener tamaño de pantalla */
    SystemTable->ConOut->QueryMode(
        SystemTable->ConOut,
        SystemTable->ConOut->Mode->Mode,
        &cols,
        &rows
    );

    /* posición random */
    UINTN x = rand_simple() % cols;
    UINTN y = rand_simple() % rows;

    /* mover cursor */
    SystemTable->ConOut->SetCursorPosition(SystemTable->ConOut, x, y);

    SystemTable->ConOut->OutputString(SystemTable->ConOut, (UINT16*)L"GIANCARLO SERGIO\r\n");
}
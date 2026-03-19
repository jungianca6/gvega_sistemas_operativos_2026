#include "game.h"

#define SCAN_UP    1
#define SCAN_DOWN  2
#define SCAN_RIGHT 3
#define SCAN_LEFT  4


static unsigned int seed = 1234567;

unsigned int rand_simple(){
    seed = seed * 1103515245 + 12345;
    return seed;
}


void draw_normal(EFI_SYSTEM_TABLE *SystemTable){
    SystemTable->ConOut->OutputString(
        SystemTable->ConOut,
        (UINT16*)L"GIANCARLO SERGIO"
    );
}

void draw_reverse(EFI_SYSTEM_TABLE *SystemTable){
    SystemTable->ConOut->OutputString(
        SystemTable->ConOut,
        (UINT16*)L"OIGRES OLRACNAIG"
    );
}

void draw_vertical(EFI_SYSTEM_TABLE *SystemTable, UINTN x, UINTN y){

    UINT16 *text = (UINT16*)L"GIANCARLO SERGIO";

    for(int i = 0; text[i] != 0; i++){

        SystemTable->ConOut->SetCursorPosition(
            SystemTable->ConOut,
            x,
            y + i
        );

        UINT16 c[2] = { text[i], 0 };

        SystemTable->ConOut->OutputString(
            SystemTable->ConOut,
            c
        );
    }
}

void draw_vertical_reverse(EFI_SYSTEM_TABLE *SystemTable, UINTN x, UINTN y){

    UINT16 *text = (UINT16*)L"OIGRES OLRACNAIG";

    for(int i = 0; text[i] != 0; i++){

        SystemTable->ConOut->SetCursorPosition(
            SystemTable->ConOut,
            x,
            y + i
        );

        UINT16 c[2] = { text[i], 0 };

        SystemTable->ConOut->OutputString(
            SystemTable->ConOut,
            c
        );
    }
}


void start_game(EFI_SYSTEM_TABLE *SystemTable){

    UINTN cols;
    UINTN rows;

    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);

    /* usar el reloj como seed */
    EFI_TIME time;
    SystemTable->RuntimeServices->GetTime(&time, NULL);

    seed ^= time.Nanosecond;
    seed ^= (time.Second << 16);
    

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

    /* estado de orientación */
    int orientation = 0;

    while(1){

        SystemTable->ConOut->ClearScreen(SystemTable->ConOut);

        if(orientation == 0){

            SystemTable->ConOut->SetCursorPosition(
                SystemTable->ConOut,
                x,
                y
            );

            draw_normal(SystemTable);
        }

        else if(orientation == 1){

            draw_vertical(SystemTable,x,y);
        }

        else if(orientation == 2){

            SystemTable->ConOut->SetCursorPosition(
                SystemTable->ConOut,
                x,
                y
            );

            draw_reverse(SystemTable);
        }

        else if(orientation == 3){

            draw_vertical_reverse(SystemTable,x,y);
        }

        EFI_INPUT_KEY key;

        while(SystemTable->ConIn->ReadKeyStroke(
            SystemTable->ConIn,
            &key
        ) != EFI_SUCCESS);

        switch(key.ScanCode){

            /* 90° izquierda */
            case SCAN_LEFT:
                orientation = (orientation + 3) % 4;
            break;

            /* 90° derecha */
            case SCAN_RIGHT:
                orientation = (orientation + 1) % 4;
            break;

            /* 180° horizontal */
            case SCAN_UP:
            case SCAN_DOWN:
                orientation = (orientation + 2) % 4;
            break;

            default:
            break;
        }

        if(key.UnicodeChar == 'r' || key.UnicodeChar == 'R')
            return;
    }
}
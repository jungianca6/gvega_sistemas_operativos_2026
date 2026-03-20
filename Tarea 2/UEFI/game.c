#include "game.h"

/*
 * Códigos de scan utilizados por UEFI para las flechas.
 * Estos valores corresponden a los ScanCode generados
 * por el firmware cuando se presionan las teclas de dirección.
 */

#define SCAN_UP    1
#define SCAN_DOWN  2
#define SCAN_RIGHT 3
#define SCAN_LEFT  4


/*
 * Semilla del generador pseudoaleatorio.
 * Se inicializa con un valor fijo y luego se mezcla
 * con el tiempo del sistema UEFI.
 */
static unsigned int seed = 1234567;

/*
 * rand_simple()
 *
 * Generador pseudoaleatorio basado en un
 * Linear Congruential Generator (LCG).
 *
 * Fórmula:
 *     seed = seed * a + c
 *
 * Se utiliza únicamente para elegir una posición
 * inicial aleatoria en pantalla.
 */
unsigned int rand_simple(){
    seed = seed * 1103515245 + 12345;
    return seed;
}

/* =============================================================
   FUNCIONES DE DIBUJO
   ============================================================= */

/*
 * draw_normal()
 *
 * Imprime el nombre en orientación normal.
 */
void draw_normal(EFI_SYSTEM_TABLE *SystemTable){
    SystemTable->ConOut->OutputString(
        SystemTable->ConOut,
        (UINT16*)L"GIANCARLO SERGIO"
    );
}

/*
 * draw_reverse()
 *
 * Imprime el nombre invertido (180° horizontal).
 */
void draw_reverse(EFI_SYSTEM_TABLE *SystemTable){
    SystemTable->ConOut->OutputString(
        SystemTable->ConOut,
        (UINT16*)L"OIGRES OLRACNAIG"
    );
}

/*
 * draw_vertical()
 *
 * Imprime el texto de forma vertical carácter por carácter.
 *
 * Cada letra se posiciona usando SetCursorPosition().
 * Esto simula una rotación de 90°.
 */

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

/*
 * draw_vertical_reverse()
 *
 * Versión vertical del texto invertido.
 */
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

/*
 * start_game()
 *
 * Bucle principal del programa interactivo.
 *
 * Funcionalidad:
 *   - obtiene tamaño de pantalla
 *   - posiciona el texto en un lugar aleatorio
 *   - rota el texto según las teclas presionadas
 *
 * Rotaciones:
 *   LEFT  -> 90° izquierda
 *   RIGHT -> 90° derecha
 *   UP    -> 180°
 *   DOWN  -> 180°
 *
 * Presionar 'R' termina el juego y retorna al menú.
 */

void start_game(EFI_SYSTEM_TABLE *SystemTable){

    UINTN cols;
    UINTN rows;

    /* limpiar pantalla */
    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);

    /* usar el reloj como seed para mejorar semilla del rand*/
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

    /*
     * estado de orientación
     *
     * 0 = normal
     * 1 = vertical
     * 2 = invertido
     * 3 = vertical invertido
     */
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
        /*
         * leer tecla del usuario
         */
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
        /*
         * volver al menú principal
         */
        if(key.UnicodeChar == 'r' || key.UnicodeChar == 'R')
            return;
    }
}
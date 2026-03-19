#include "game.h"

typedef unsigned char       UINT8;
typedef unsigned short      UINT16;
typedef unsigned int        UINT32;
typedef unsigned long long  UINT64;
typedef long long           INT64;
typedef void*               EFI_HANDLE;
typedef UINT64              EFI_STATUS;
typedef UINT64              UINTN;

#define EFI_SUCCESS 0
#define NULL        ((void*)0)

typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8]; 
} EFI_GUID;


typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

typedef EFI_STATUS (*EFI_TEXT_RESET)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    int ExtendedVerification);


typedef EFI_STATUS (*EFI_TEXT_STRING)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINT16 *String);


typedef EFI_STATUS (*EFI_TEXT_CLEAR_SCREEN)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This);

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL{
    EFI_TEXT_RESET Reset;
    EFI_TEXT_STRING OutputString;
    void *TestString;
    void *QueryMode;
    void *SetMode;
    void *SetAttribute;
    EFI_TEXT_CLEAR_SCREEN ClearScreen;
};

/* =========================
   TECLADO
   ========================= */

typedef struct {
    UINT16 ScanCode;
    UINT16 UnicodeChar;
} EFI_INPUT_KEY;

typedef EFI_STATUS (*EFI_INPUT_RESET)(
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
    int ExtendedVerification);

typedef EFI_STATUS (*EFI_INPUT_READ_KEY)(
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
    EFI_INPUT_KEY *Key);

struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    EFI_INPUT_RESET Reset;
    EFI_INPUT_READ_KEY ReadKeyStroke;
    void *WaitForKey;
};

/* ========================= /
/ SYSTEM TABLE /
/ ========================= */

typedef struct {
    char _pad1[48];
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *ConIn;
    void                            *ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;

} EFI_SYSTEM_TABLE;

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

    get_key(SystemTable);

    goto start;
}
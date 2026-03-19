#include "game.h"

typedef unsigned short UINT16;

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct {
    char _pad1[48];
    void *ConIn;
    void *ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
} EFI_SYSTEM_TABLE;

typedef unsigned long long EFI_STATUS;

typedef EFI_STATUS (*EFI_TEXT_STRING)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINT16 *String);

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL{
    void *Reset;
    EFI_TEXT_STRING OutputString;
};

void print(EFI_SYSTEM_TABLE *SystemTable, UINT16 *str){
    SystemTable->ConOut->OutputString(SystemTable->ConOut, str);
}

void start_game(EFI_SYSTEM_TABLE *SystemTable){

    print(SystemTable, (UINT16*)L"Giancarlo\r\n");
}
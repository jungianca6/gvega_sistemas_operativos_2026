#ifndef EFI_TYPES_H
#define EFI_TYPES_H

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

typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    UINT32 Attribute;
    UINT32 CursorColumn;
    UINT32 CursorRow;
    int CursorVisible;
} SIMPLE_TEXT_OUTPUT_MODE;

typedef struct {
    UINT16 Year;
    UINT8  Month;
    UINT8  Day;
    UINT8  Hour;
    UINT8  Minute;
    UINT8  Second;
    UINT8  Pad1;
    UINT32 Nanosecond;
    UINT16  TimeZone;
    UINT8  Daylight;
    UINT8  Pad2;
} EFI_TIME;

// Forward declarations
typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

// Output Protocol
typedef EFI_STATUS (*EFI_TEXT_RESET)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    int ExtendedVerification);

typedef EFI_STATUS (*EFI_TEXT_STRING)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINT16 *String);

typedef EFI_STATUS (*EFI_TEXT_CLEAR_SCREEN)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This);


typedef EFI_STATUS (*EFI_TEXT_QUERY_MODE)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN ModeNumber,
    UINTN *Columns,
    UINTN *Rows
);

typedef EFI_STATUS (*EFI_TEXT_SET_CURSOR_POSITION)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN Column,
    UINTN Row
);

typedef EFI_STATUS (*EFI_GET_TIME)(
    EFI_TIME *Time,
    void *Capabilities
);

typedef struct {
    char _pad[24];
    EFI_GET_TIME GetTime;
} EFI_RUNTIME_SERVICES;

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL{
    EFI_TEXT_RESET Reset;
    EFI_TEXT_STRING OutputString;
    void *TestString;

    EFI_TEXT_QUERY_MODE QueryMode;
    void *SetMode;
    void *SetAttribute;

    EFI_TEXT_CLEAR_SCREEN ClearScreen;
    EFI_TEXT_SET_CURSOR_POSITION     SetCursorPosition;

    void *EnableCursor;
    SIMPLE_TEXT_OUTPUT_MODE *Mode;
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

// System Table
typedef struct {
    char _pad1[48];
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *ConIn;
    void                            *ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    void                            *StdErrHandle;
    void                            *StdErr;
    EFI_RUNTIME_SERVICES            *RuntimeServices;
} EFI_SYSTEM_TABLE;

#endif
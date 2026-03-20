%include "stage2info.inc"
ORG STAGE2_RUN_OFS

BITS 16

; ============================================================
; CONSTANTS
; ============================================================
SCREEN_COLS     equ 80
SCREEN_ROWS     equ 25

KEY_UP          equ 0x48
KEY_DOWN        equ 0x50
KEY_LEFT        equ 0x4B
KEY_RIGHT       equ 0x4D

; Colour attributes
ATTR_MENU_FILL  equ 0x0B    ; Bright Cyan on Black (menu background)
ATTR_NAME       equ 0x0A    ; Bright Green on Black
ATTR_STATUS     equ 0x07    ; Grey on Black

; "SergioGiancarlo" = 15 characters, pivot index = 7 (the 'G')
NAME_LEN        equ 15
NAME_CTR        equ 7       ; half-length  (pivot offset from either end)

; Safe anchor ranges so the full string always fits on screen:
;   horizontal: col in [7 .. 72]  (margins of NAME_CTR on each side)
;   vertical:   row in [7 .. 15]  (margins of NAME_CTR, leave row 24 for status)
COL_MIN         equ NAME_CTR                    ; 7
COL_RANGE       equ SCREEN_COLS - 1 - (2 * NAME_CTR)   ; 65
ROW_MIN         equ NAME_CTR                    ; 7
ROW_RANGE       equ SCREEN_ROWS - 2 - (2 * NAME_CTR)   ; 9

; ============================================================
; ENTRY POINT  ─ Main Menu
; ============================================================
start:
    ; Fill entire screen Cyan on Black
    mov ah, 06h
    xor al, al
    xor cx, cx
    mov dx, 184Fh
    mov bh, ATTR_MENU_FILL
    int 10h

    ; "My name" (7 chars) centred → col = (80-7)/2 = 36, row 11
    mov dh, 11
    mov dl, 36
    call set_cursor
    mov si, str_title
    call print_str

    ; "Press Enter to continue" (23 chars) → col = (80-23)/2 = 28, row 13
    mov dh, 13
    mov dl, 28
    call set_cursor
    mov si, str_enter
    call print_str

.wait_enter:
    mov ah, 0h
    int 16h
    cmp ah, 0x1C        ; Enter scan code
    jne .wait_enter
    ; fall into game_init

; ============================================================
; GAME INIT  ─ randomise anchor, reset orient, render
; ============================================================
game_init:
    call cls_black

    ; Random column for anchor
    call timer_rand         ; AL = pseudo-random byte
    xor ah, ah
    mov bl, COL_RANGE
    div bl                  ; AH = remainder ∈ [0, COL_RANGE)
    add ah, COL_MIN         ; shift into [COL_MIN, COL_MIN+COL_RANGE)
    mov [anchor_col], ah

    ; Random row for anchor
    call timer_rand
    xor ah, ah
    mov bl, ROW_RANGE
    div bl
    add ah, ROW_MIN
    mov [anchor_row], ah

    mov byte [orient], 0    ; start in normal orientation

    call render
    ; fall into game_loop

; ============================================================
; GAME LOOP
; ============================================================
game_loop:
    mov ah, 0h
    int 16h             ; AH = scan code, AL = ASCII

    cmp al, 'E'
    je game_end
    cmp al, 'e'
    je game_end

    cmp al, 'R'
    je game_init
    cmp al, 'r'
    je game_init

    ; Arrow keys: AL == 0 (extended scan code in AH)
    test al, al
    jnz game_loop

    cmp ah, KEY_RIGHT
    je do_right
    cmp ah, KEY_LEFT
    je do_left
    cmp ah, KEY_UP
    je do_up
    cmp ah, KEY_DOWN
    je do_down

    jmp game_loop

do_right:
    mov al, [orient]
    inc al
    and al, 3
    mov [orient], al
    call render
    jmp game_loop

do_left:
    mov al, [orient]
    add al, 3           ; -1 mod 4
    and al, 3
    mov [orient], al
    call render
    jmp game_loop

do_up:
    mov al, [orient]
    add al, 2           ; 180° = 2 steps
    and al, 3
    mov [orient], al
    call render
    jmp game_loop

do_down:
    mov al, [orient]
    add al, 2
    and al, 3
    mov [orient], al
    call render
    jmp game_loop

; ============================================================
; GAME END
; ============================================================
game_end:
    call cls_black
    mov dh, 12
    mov dl, 36
    call set_cursor
    mov si, str_bye
    call print_str
    mov ah, 0h
    int 16h
    jmp start

; ============================================================
; RENDER
; ============================================================
; orient meanings and drawing rules  (pivot char = index 7 = 'G'):
;
;   0 = NORMAL     horizontal L→R
;       char i  →  row = anchor_row,          col = anchor_col - NAME_CTR + i
;
;   1 = ROT RIGHT  vertical top→bottom
;       char i  →  row = anchor_row - NAME_CTR + i,  col = anchor_col
;
;   2 = FLIPPED    horizontal R→L  (reversed order)
;       char i  →  row = anchor_row,          col = anchor_col + NAME_CTR - i
;
;   3 = ROT LEFT   vertical bottom→top
;       char i  →  row = anchor_row + NAME_CTR - i,  col = anchor_col
;
; In every case index 7 lands on (anchor_row, anchor_col).
;
; All registers modified locally; uses no stack variables.
; ============================================================
render:
    pusha
    call cls_black

    ; -- prepare loop variables --------------------------------
    ;   DH = current row,  DL = current col
    ;   SI = pointer into str_name
    ;   CX = chars remaining

    mov al, [orient]

    ; compute starting row and col for i=0
    cmp al, 0
    je .prep_normal
    cmp al, 1
    je .prep_rotright
    cmp al, 2
    je .prep_flipped
    ; else orient==3
    jmp .prep_rotleft

; ── orient 0: row = anchor_row, col_start = anchor_col - 7 ──
.prep_normal:
    mov dh, [anchor_row]
    mov dl, [anchor_col]
    sub dl, NAME_CTR            ; col of first char
    mov si, str_name
    mov cx, NAME_LEN
    ; row delta=0, col delta=+1
    mov byte [d_row], 0
    mov byte [d_col], 1
    jmp .draw_loop

; ── orient 1: col = anchor_col, row_start = anchor_row - 7 ──
.prep_rotright:
    mov dh, [anchor_row]
    sub dh, NAME_CTR
    mov dl, [anchor_col]
    mov si, str_name
    mov cx, NAME_LEN
    mov byte [d_row], 1
    mov byte [d_col], 0
    jmp .draw_loop

; ── orient 2: row = anchor_row, col_start = anchor_col + 7 ──
.prep_flipped:
    mov dh, [anchor_row]
    mov dl, [anchor_col]
    add dl, NAME_CTR
    mov si, str_name
    mov cx, NAME_LEN
    mov byte [d_row], 0
    mov byte [d_col], 0xFF      ; -1 (move left)
    jmp .draw_loop

; ── orient 3: col = anchor_col, row_start = anchor_row + 7 ──
.prep_rotleft:
    mov dh, [anchor_row]
    add dh, NAME_CTR
    mov dl, [anchor_col]
    mov si, str_name
    mov cx, NAME_LEN
    mov byte [d_row], 0xFF      ; -1 (move up)
    mov byte [d_col], 0
    ; fall into draw_loop

; ── unified draw loop ─────────────────────────────────────────
; DH=row, DL=col, SI=string ptr, CX=count
; d_row/d_col = signed step per iteration
.draw_loop:
    ; set cursor to (DH, DL)
    mov ah, 02h
    xor bh, bh              ; page 0
    int 10h

    ; write character with attribute
    lodsb                   ; AL = next char from str_name
    mov ah, 09h
    mov bx, ATTR_NAME       ; BH=0 (page 0), BL=attribute
    mov cx, 1
    int 10h

    ; advance position
    mov al, [d_row]
    add dh, al
    mov al, [d_col]
    add dl, al

    ; reload cx for next iteration counter
    ; (CX was clobbered by INT 10h/09h – use separate counter)
    dec byte [loop_cnt]
    jnz .draw_loop

    ; reset counter for next call
    mov byte [loop_cnt], NAME_LEN

    ; ── status bar ───────────────────────────────────────────
    mov dh, SCREEN_ROWS - 1
    mov dl, 0
    call set_cursor
    mov si, str_status
    call print_str

    popa
    ret

; ============================================================
; UTILITIES
; ============================================================

cls_black:
    pusha
    mov ah, 06h
    xor al, al
    xor cx, cx
    mov dx, 184Fh
    mov bh, 0x00
    int 10h
    mov ah, 02h
    xor bh, bh
    xor dx, dx
    int 10h
    popa
    ret

set_cursor:                 ; DH=row, DL=col
    push ax
    push bx
    mov ah, 02h
    xor bh, bh
    int 10h
    pop bx
    pop ax
    ret

print_str:                  ; print NUL-terminated string at current cursor
    push ax
    push bx
    mov ah, 0Eh
    xor bx, bx
.ps_lp:
    lodsb
    test al, al
    jz .ps_done
    int 10h
    jmp .ps_lp
.ps_done:
    pop bx
    pop ax
    ret

timer_rand:                 ; returns pseudo-random byte in AL
    push bx
    push cx
    push dx
    xor ax, ax
    int 1Ah                 ; CX:DX = BIOS tick count
    mov al, dl
    xor al, dh
    xor al, cl
    xor al, ch
    pop dx
    pop cx
    pop bx
    ret

; ============================================================
; DATA
; ============================================================
str_title   db 'My name', 0
str_enter   db 'Press Enter to continue', 0
str_name    db 'SergioGiancarlo', 0     ; 15 chars, pivot='G' at index 7
str_status  db 'Left/Right: Rotate 90    Up/Down: Flip 180    R: Restart    E: Exit', 0
str_bye     db 'Goodbye!', 0

; State variables
anchor_row  db 12
anchor_col  db 40
orient      db 0    ; 0=normal 1=rot-right 2=flipped 3=rot-left

; Draw loop helpers (avoids corrupting CX across INT 10h calls)
loop_cnt    db NAME_LEN
d_row       db 0    ; row step  (+1, -1, or 0)
d_col       db 1    ; col step  (+1, -1, or 0)
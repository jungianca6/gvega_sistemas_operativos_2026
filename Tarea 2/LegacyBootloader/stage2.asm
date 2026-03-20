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

ATTR_MENU       equ 0x0B    ; Bright Cyan on Black
ATTR_NAME       equ 0x0A    ; Bright Green on Black
ATTR_STATUS     equ 0x07    ; Grey on Black

; "SergioGiancarlo" = 15 chars, pivot = index 7 ('G')
NAME_LEN        equ 15
NAME_CTR        equ 7

COL_MIN         equ NAME_CTR
COL_RANGE       equ SCREEN_COLS - 1 - (2 * NAME_CTR)   ; 65
ROW_MIN         equ NAME_CTR
ROW_RANGE       equ SCREEN_ROWS - 2 - (2 * NAME_CTR)   ; 9

; ============================================================
; MAIN MENU
; ============================================================
start:
    mov ah, 06h
    xor al, al
    xor cx, cx
    mov dx, 184Fh
    mov bh, ATTR_MENU
    int 10h

    ; "My name" row 11 col 36
    mov dh, 11
    mov dl, 36
    call set_cursor
    mov si, str_title
    mov bl, ATTR_MENU
    call print_attr

    ; "Press Enter to continue" row 13 col 28
    mov dh, 13
    mov dl, 28
    call set_cursor
    mov si, str_enter
    mov bl, ATTR_MENU
    call print_attr

.wait_enter:
    mov ah, 0h
    int 16h
    cmp ah, 0x1C
    jne .wait_enter

; ============================================================
; GAME INIT
; ============================================================
game_init:
    call cls_black

    call timer_rand
    xor ah, ah
    mov bl, COL_RANGE
    div bl
    add ah, COL_MIN
    mov [anchor_col], ah

    call timer_rand
    xor ah, ah
    mov bl, ROW_RANGE
    div bl
    add ah, ROW_MIN
    mov [anchor_row], ah

    mov byte [orient], 0
    call render

; ============================================================
; GAME LOOP
; ============================================================
game_loop:
    mov ah, 0h
    int 16h

    cmp al, 'E'
    je start
    cmp al, 'e'
    je start

    cmp al, 'R'
    je game_init
    cmp al, 'r'
    je game_init

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
    add al, 3
    and al, 3
    mov [orient], al
    call render
    jmp game_loop

do_up:
    mov al, [orient]
    add al, 2
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
; RENDER
; ============================================================
render:
    pusha
    call cls_black

    mov al, [orient]
    cmp al, 0
    je .prep_normal
    cmp al, 1
    je .prep_rotright
    cmp al, 2
    je .prep_flipped
    jmp .prep_rotleft

.prep_normal:
    mov dh, [anchor_row]
    mov dl, [anchor_col]
    sub dl, NAME_CTR
    mov byte [d_row], 0
    mov byte [d_col], 1
    jmp .draw_loop

.prep_rotright:
    mov dh, [anchor_row]
    sub dh, NAME_CTR
    mov dl, [anchor_col]
    mov byte [d_row], 1
    mov byte [d_col], 0
    jmp .draw_loop

.prep_flipped:
    mov dh, [anchor_row]
    mov dl, [anchor_col]
    add dl, NAME_CTR
    mov byte [d_row], 0
    mov byte [d_col], 0xFF
    jmp .draw_loop

.prep_rotleft:
    mov dh, [anchor_row]
    add dh, NAME_CTR
    mov dl, [anchor_col]
    mov byte [d_row], 0xFF
    mov byte [d_col], 0

.draw_loop:
    mov si, str_name
    mov byte [loop_cnt], NAME_LEN

.draw_char:
    ; Position cursor at (DH, DL)
    mov ah, 02h
    xor bh, bh
    int 10h

    ; Write character with explicit attribute (does not move cursor)
    lodsb
    mov ah, 09h
    mov bx, ATTR_NAME
    mov cx, 1
    int 10h

    ; Advance position
    mov al, [d_row]
    add dh, al
    mov al, [d_col]
    add dl, al

    dec byte [loop_cnt]
    jnz .draw_char

    ; ── Status bar ───────────────────────────────────────────
    ; Each character printed with INT 10h/09h + manual column
    ; advance so ATTR_STATUS is always respected.
    mov dh, SCREEN_ROWS - 1
    mov dl, 0
    mov si, str_status

.status_char:
    lodsb
    test al, al
    jz .render_done

    mov ah, 02h
    xor bh, bh
    int 10h

    mov ah, 09h
    mov bx, ATTR_STATUS
    mov cx, 1
    int 10h

    inc dl
    cmp dl, SCREEN_COLS
    jb .status_char

.render_done:
    popa
    ret

; ============================================================
; UTILITIES
; ============================================================

; cls_black — fill screen black-on-black, home cursor
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

; set_cursor — DH=row, DL=col
set_cursor:
    push ax
    push bx
    mov ah, 02h
    xor bh, bh
    int 10h
    pop bx
    pop ax
    ret

; print_attr — print NUL-terminated string at current cursor.
;   SI = string pointer
;   BL = colour attribute
;   DL = starting column (caller must set via set_cursor first)
; Uses INT 10h/09h + manual DL advance so colour is always explicit.
print_attr:
    push ax
    push bx
    push cx

    ; Save BL: some BIOSes clobber BX on INT 10h/09h.
    ; We keep attr in [pa_attr] and reload BL each iteration.
    mov [pa_attr], bl

.pa_loop:
    lodsb
    test al, al
    jz .pa_done

    ; Write char with colour
    mov ah, 09h
    mov bl, [pa_attr]
    xor bh, bh
    mov cx, 1
    int 10h

    ; Advance cursor column
    inc dl
    mov ah, 02h
    xor bh, bh
    int 10h

    jmp .pa_loop

.pa_done:
    pop cx
    pop bx
    pop ax
    ret

; timer_rand — pseudo-random byte in AL (BIOS tick counter mix)
timer_rand:
    push bx
    push cx
    push dx
    xor ax, ax
    int 1Ah
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
str_name    db 'SergioGiancarlo', 0
str_status  db 'Left/Right:Rotate 90  Up/Down:Flip 180  R:Restart  E:Menu', 0

anchor_row  db 12
anchor_col  db 40
orient      db 0

loop_cnt    db NAME_LEN
d_row       db 0
d_col       db 1
pa_attr     db 0
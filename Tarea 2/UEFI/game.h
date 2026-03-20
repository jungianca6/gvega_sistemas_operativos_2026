/*
===========================================================
GAME MODULE
===========================================================

Este módulo define la interfaz pública del juego que se
ejecuta dentro del entorno UEFI.
El archivo game.c contiene la implementación de estas
funciones.

Dependencias:
- efitypes.h : definiciones de estructuras y tipos UEFI
===========================================================
*/
#ifndef GAME_H
#define GAME_H
#include "efitypes.h"

#include <stdint.h>

void start_game(EFI_SYSTEM_TABLE *SystemTable);

#endif
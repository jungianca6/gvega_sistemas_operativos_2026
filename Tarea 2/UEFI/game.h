#ifndef GAME_H
#define GAME_H

#include <stdint.h>

typedef unsigned short UINT16;

struct EFI_SYSTEM_TABLE;

void start_game(struct EFI_SYSTEM_TABLE *SystemTable);

#endif
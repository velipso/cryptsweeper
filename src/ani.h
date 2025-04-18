//
// cryptsweeper - fight the graveyard monsters and stop death
// by Pocket Pulp (@velipso), https://pulp.biz
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#pragma once
#include "sys.h"

typedef struct {
  const u16 *pc;
  u8 waitcount;
  u8 loopcount;
  i16 gravity; // Q8.8
  struct { // Q16.0
    i16 x;
    i16 y;
  } origin;
  struct { // Q8.8
    i16 x;
    i16 y;
    i16 dx;
    i16 dy;
  } offset;
} sprite_st;

extern u16 g_oam[0x200];
extern sprite_st g_sprites[128];

void ani_flushxy(u32 i);
void ani_step(u32 i);

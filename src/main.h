//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#pragma once
#include "sys.h"

extern u32 rnd_seed;
extern u32 rnd_i;

BINFILE(palette_brightness_bin);
BINFILE(tiles_bin);
BINFILE(ui_bin);
BINFILE(sprites_bin);
BINFILE(song1_gvsong);
BINFILE(levels_bin);
BINFILE(popups_bin);
BINFILE(scr_title_o);
BINFILE(scr_winmine_o);
BINFILE(scr_wingame_o);

void gvmain();

static inline u32 whisky2(u32 i0, u32 i1){
  u32 z0 = (i1 * 1833778363) ^ i0;
  u32 z1 = (z0 *  337170863) ^ (z0 >> 13) ^ z0;
  u32 z2 = (z1 *  620363059) ^ (z1 >> 10);
  u32 z3 = (z2 *  232140641) ^ (z2 >> 21);
  return z3;
}

static inline u32 rnd32() {
  return whisky2(rnd_seed, ++rnd_i);
}

//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#pragma once
#include "sys.h"
#include "rnd.h"

extern struct rnd_st g_rnd;

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

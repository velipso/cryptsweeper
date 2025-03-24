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
BINFILE(lava_bin);
BINFILE(song1_gvsong);
BINFILE(song2_gvsong);
BINFILE(levels_bin);
BINFILE(popups_bin);
BINFILE(scr_title_o);
BINFILE(scr_winmine_o);
BINFILE(scr_wingame_o);
BINFILE(scr_how1_o);
BINFILE(scr_how2_o);
BINFILE(scr_how3_o);
BINFILE(scr_book_items1_o);
BINFILE(scr_book_items2_o);
BINFILE(scr_book_lowlevel_o);
BINFILE(scr_book_lv1b_o);
BINFILE(scr_book_lv3a_o);
BINFILE(scr_book_lv3bc_o);
BINFILE(scr_book_lv4a_o);
BINFILE(scr_book_lv4b_o);
BINFILE(scr_book_lv4c_o);
BINFILE(scr_book_lv5a_o);
BINFILE(scr_book_lv5b_o);
BINFILE(scr_book_lv5c_o);
BINFILE(scr_book_lv6_o);
BINFILE(scr_book_lv7_o);
BINFILE(scr_book_lv8_o);
BINFILE(scr_book_lv9_o);
BINFILE(scr_book_lv10_o);
BINFILE(scr_book_lv11_o);
BINFILE(scr_book_lv13_o);
BINFILE(scr_book_mine_o);
BINFILE(scr_book_wall_o);
BINFILE(scr_book_chest_o);
BINFILE(scr_book_lava_o);

void gvmain();

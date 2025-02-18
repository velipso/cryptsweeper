//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#include "main.h"
#include <stdlib.h>
#include "ani.h"
#include "anidata.h"
#include "sfx.h"
#include "util.h"
#include "levelgen.h"

#define S_CURSOR1    0
#define S_CURSOR2    1
#define S_CURSOR3    2
#define S_CURSOR4    3
#define S_HP_START   4
#define S_HP_END     21
#define S_EXP_START  22
#define S_EXP_END    51

static u32 g_down;
static u32 g_hit;
static i32 g_cursor_x;
static i32 g_cursor_y;
static i32 g_statsel_x;
static i32 g_statsel_y;

struct game_st {
  u32 seed;
  u32 rnd;
  u32 books;
  u8 songvol;
  u8 sfxvol;
  u8 brightness;
  u8 selx;
  u8 sely;
  u8 difficulty;
  u8 level;
  u8 hp;
  u8 exp;
  u8 board[BOARD_SIZE];
  // SSTT:TTTT
  // where SS is status:
  //   00 - unpressed, hidden
  //   01 - unpressed, visible
  //   10 - pressed (if not empty, then collectable)
  // and TT is type (0-63)
};

struct game_st game;
u32 rnd_seed = 1;
u32 rnd_i = 1;

// map 0-10 -> 0-16
static const u16 volume_map_fwd[] SECTION_ROM =
  {0, 1, 2, 3, 5, 6, 8, 10, 12, 14, 16};
// map 0-16 -> 0-10
static const u16 volume_map_back[] SECTION_ROM =
  {0, 1, 2, 3, 3, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10};

static void SECTION_IWRAM_ARM irq_vblank() {
  sys_copy_oam(g_oam);
  u32 inp = sys_input() ^ 0x3ff;
  g_hit = ~g_down & inp;
  g_down = inp;
  rnd_seed = whisky2(rnd_seed, inp);
}

static void nextframe() {
  for (int i = 0; i < 128; i++)
    ani_step(i);
  sys_nextframe();
}

static u32 board_type_to_tile(u32 type) {
  #define TILE(px, py)  (px >> 3) + ((py >> 3) << 5)
  switch (GET_TYPE(type)) {
    case T_EMPTY:      return 0;
    case T_LV1A:       return TILE(160, 0);
    case T_LV1B:       return TILE(160, 16);
    case T_LV2:        return TILE(160, 32);
    case T_LV3:        return TILE(160, 48);
    case T_LV4:        return TILE(160, 64);
    case T_LV5A:       return TILE(160, 80);
    case T_LV5B:       return TILE(160, 96);
    case T_LV5C:       return TILE(160, 112);
    case T_LV6:        return TILE(208, 0);
    case T_LV7A:       return TILE(208, 16);
    case T_LV7B:       return TILE(208, 16);
    case T_LV7C:       return TILE(208, 16);
    case T_LV7D:       return TILE(208, 16);
    case T_LV8:        return TILE(208, 32);
    case T_LV9:        return TILE(208, 48);
    case T_LV10:       return TILE(208, 64);
    case T_LV11:       return TILE(208, 80);
    case T_LV13:       return TILE(208, 96);
    case T_MINE:       return TILE(64, 0);
    case T_WALL:       return TILE(64, 32);
    case T_CHEST_HEAL: return TILE(64, 16);
    case T_CHEST_EYE:  return TILE(64, 16);
    case T_CHEST_EXP:  return TILE(64, 16);
    case T_ITEM_HEAL:  return TILE(0, 112);
    case T_ITEM_EYE:   return TILE(16, 112);
    case T_ITEM_SHOW1: return TILE(32, 112);
    case T_ITEM_SHOW5: return TILE(48, 112);
    case T_ITEM_EXP1:  return TILE(64, 112);
    case T_ITEM_EXP3:  return TILE(80, 112);
    case T_ITEM_EXP5:  return TILE(96, 112);
  }
  return 0;
  #undef TILE
}

static u32 dig99(i32 num) {
  if (num < 0) num = 0;
  if (num > 99) num = 99;
  u32 d1 =
    num < 20 ? 1 :
    num < 30 ? 2 :
    num < 40 ? 3 :
    num < 50 ? 4 :
    num < 60 ? 5 :
    num < 70 ? 6 :
    num < 80 ? 7 :
    num < 90 ? 8 : 9;
  u32 d2 = num - d1 * 10;
  return (d1 << 4) | d2;
}

// -3 = blank
// -2 = mine
// -1 = ?
// 0-99 = number
static void place_number(i32 x, i32 y, u32 style, i32 num) {
  u32 offset = (2 + x * 2) * 2 + (2 + y * 2) * 64;
  if (num < -2) {
    sys_set_map(0x1d, offset + 0, 0);
    sys_set_map(0x1d, offset + 2, 0);
  } else if (num == -2) {
    sys_set_map(0x1d, offset + 0, 6);
    sys_set_map(0x1d, offset + 2, 7);
  } else if (num < 10) {
    if (num < 10) {
      num = 66 + 32 * num;
    }
    sys_set_map(0x1d, offset + 0, style + num);
    sys_set_map(0x1d, offset + 2, style + num + 1);
  } else {
    u32 d12 = dig99(num);
    u32 d1 = 64 + 32 * (d12 >> 4);
    u32 d2 = 65 + 32 * (d12 & 15);
    sys_set_map(0x1d, offset + 0, style + d1);
    sys_set_map(0x1d, offset + 2, style + d2);
  }
}

static void place_minehint(i32 x, i32 y, i32 num) {
  u32 offset = (2 + x * 2) * 2 + (3 + y * 2) * 64;
  switch (num) {
    case 0: num = 0; break;
    case 1: num = 32; break;
    case 2: num = 2; break;
    case 3: num = 4; break;
    case 4: num = 36; break;
    default: num = 0; break;
  }
  sys_set_map(0x1d, offset + 0, num);
  sys_set_map(0x1d, offset + 2, num == 0 ? 0 : num + 1);
}

static void place_answer(i32 x, i32 y, i32 frame) {
  frame *= 2;
  u32 t = board_type_to_tile(game.board[x + y * BOARD_W]);
  u32 offset = (x + 1) * 4 + (y + 1) * 128;
  if (t == 0) {
    sys_set_map(0x1e, offset +  0, 0);
    sys_set_map(0x1e, offset +  2, 0);
    sys_set_map(0x1e, offset + 64, 0);
    sys_set_map(0x1e, offset + 66, 0);
  } else {
    sys_set_map(0x1e, offset +  0, frame + t +  0);
    sys_set_map(0x1e, offset +  2, frame + t +  1);
    sys_set_map(0x1e, offset + 64, frame + t + 32);
    sys_set_map(0x1e, offset + 66, frame + t + 33);
  }
}

static void place_floor(i32 x, i32 y, i32 state) {
  u32 offset = (x + 1) * 4 + (y + 1) * 128;
  u32 r = 7;
  for (i32 i = 0; r >= 6; i++) {
    r = whisky2(offset, i) & 7;
  }
  r = (r << 1) + state * 64;
  sys_set_map(0x1f, offset +  0, 264 + r);
  sys_set_map(0x1f, offset +  2, 265 + r);
  sys_set_map(0x1f, offset + 64, 296 + r);
  sys_set_map(0x1f, offset + 66, 297 + r);
}

static i32 threat_at(i32 x, i32 y) {
  if (x < 0 || x >= BOARD_W || y < 0 || y >= BOARD_H) return 0;
  switch (GET_TYPE(game.board[x + y * BOARD_W])) {
    case T_EMPTY:      return 0;
    case T_LV1A:       return 1;
    case T_LV1B:       return 1;
    case T_LV2:        return 2;
    case T_LV3:        return 3;
    case T_LV4:        return 4;
    case T_LV5A:       return 5;
    case T_LV5B:       return 5;
    case T_LV5C:       return 5;
    case T_LV6:        return 6;
    case T_LV7A:       return 7;
    case T_LV7B:       return 7;
    case T_LV7C:       return 7;
    case T_LV7D:       return 7;
    case T_LV8:        return 8;
    case T_LV9:        return 9;
    case T_LV10:       return 10;
    case T_LV11:       return 11;
    case T_LV13:       return 13;
    case T_MINE:       return 0x100;
  }
  return 0;
}

static i32 count_threat(i32 x, i32 y) {
  i32 result = -threat_at(x, y);
  for (i32 dy = -1; dy <= 1; dy++) {
    i32 by = dy + y;
    for (i32 dx = -1; dx <= 1; dx++) {
      i32 bx = dx + x;
      result += threat_at(bx, by);
    }
  }
  return result;
}

static void place_threat(i32 x, i32 y) {
  i32 threat = count_threat(x, y);
  place_minehint(x, y, threat >> 8);
  if (threat & 0xff) {
    place_number(x, y, 0, threat & 0xff);
  } else {
    place_number(x, y, 0, -3);
  }
}

static void cursor_hide() {
  g_sprites[S_CURSOR1].pc = NULL;
  g_sprites[S_CURSOR2].pc = NULL;
  g_sprites[S_CURSOR3].pc = NULL;
  g_sprites[S_CURSOR4].pc = NULL;
}

static void cursor_show() {
  g_sprites[S_CURSOR1].pc = ani_cursor1;
  g_sprites[S_CURSOR2].pc = ani_cursor2;
  g_sprites[S_CURSOR3].pc = ani_cursor3;
  g_sprites[S_CURSOR4].pc = ani_cursor4;
}

static void cursor_to_gamesel() {
  g_cursor_x = game.selx * 16 + 8;
  g_cursor_y = game.sely * 16 + 3;
  g_sprites[S_CURSOR1].origin.x = g_cursor_x;
  g_sprites[S_CURSOR1].origin.y = g_cursor_y;
  g_sprites[S_CURSOR2].origin.x = g_cursor_x + 16;
  g_sprites[S_CURSOR2].origin.y = g_cursor_y;
  g_sprites[S_CURSOR3].origin.x = g_cursor_x;
  g_sprites[S_CURSOR3].origin.y = g_cursor_y + 16;
  g_sprites[S_CURSOR4].origin.x = g_cursor_x + 16;
  g_sprites[S_CURSOR4].origin.y = g_cursor_y + 16;
}

static void cursor_to_statsel() {
  u32 w = g_statsel_y == 0 ? 40 : 16;
  g_cursor_x = (g_statsel_y == 0 ? g_statsel_x & ~1 : g_statsel_x) * 24 + 4;
  g_cursor_y = g_statsel_y * 24 + 56;
  g_sprites[S_CURSOR1].origin.x = g_cursor_x;
  g_sprites[S_CURSOR1].origin.y = g_cursor_y;
  g_sprites[S_CURSOR2].origin.x = g_cursor_x + w;
  g_sprites[S_CURSOR2].origin.y = g_cursor_y;
  g_sprites[S_CURSOR3].origin.x = g_cursor_x;
  g_sprites[S_CURSOR3].origin.y = g_cursor_y + 16;
  g_sprites[S_CURSOR4].origin.x = g_cursor_x + w;
  g_sprites[S_CURSOR4].origin.y = g_cursor_y + 16;
}

static u32 next_statdig_spr;
static void place_statdig(u8 dig, i32 x, const u16 *pc[]) {
  g_sprites[next_statdig_spr].pc = pc[dig];
  g_sprites[next_statdig_spr].origin.x = x;
  g_sprites[next_statdig_spr].origin.y = 147;
  next_statdig_spr++;
}

static void place_statnum(u8 cur, u8 max, i32 x, const u16 *pc[]) {
  u32 cur2 = dig99(cur);
  u32 cur1 = cur2 >> 4;
  cur2 &= 15;
  u32 max2 = dig99(max);
  u32 max1 = max2 >> 4;
  max2 &= 15;
  u32 width = 10; // width of slash+padding
  if (cur1) width += 13; // double digit
  else width += 6; // single digit
  if (max1) width += 13; // double digit
  else width += 6; // single digit
  x -= width;
  if (cur1) {
    place_statdig(cur1, x, pc);
    x += 7;
    place_statdig(cur2, x, pc);
    x += 7;
  } else {
    place_statdig(cur2, x, pc);
    x += 7;
  }
  // slash
  place_statdig(10, x, pc);
  x += 6;
  if (max1) {
    place_statdig(max1, x, pc);
    x += 7;
    place_statdig(max2, x, pc);
    x += 7;
  } else {
    place_statdig(max2, x, pc);
    x += 7;
  }
}

static void place_hp(u8 cur, u8 max) {
  const i32 hp_x = 37;
  const i32 hp_y = 150;
  if (cur <= 7) {
    // put empty in the background
    i32 hp_i = 6;
    for (i32 i = 0; i < 7; i++, hp_i--) {
      g_sprites[S_HP_START + i].pc = hp_i < max ? hp_i < cur ? ani_hpfull : ani_hpempty : NULL;
      g_sprites[S_HP_START + i].origin.x = hp_x + (6 - i) * 8;
      g_sprites[S_HP_START + i].origin.y = hp_y + 2;
    }
    for (i32 i = 7; i < 13; i++, hp_i--) {
      g_sprites[S_HP_START + i].pc = hp_i < max ? ani_hpempty : NULL;
      g_sprites[S_HP_START + i].origin.x = hp_x + 4 + (12 - i) * 8;
      g_sprites[S_HP_START + i].origin.y = hp_y;
    }
  } else {
    i32 hp_i = 12;
    for (i32 i = 0; i < 6; i++, hp_i--) {
      g_sprites[S_HP_START + i].pc = hp_i < max ? hp_i < cur ? ani_hpfull : ani_hpempty : NULL;
      g_sprites[S_HP_START + i].origin.x = hp_x + 4 + (5 - i) * 8;
      g_sprites[S_HP_START + i].origin.y = hp_y + 2;
    }
    for (i32 i = 6; i < 13; i++, hp_i--) {
      g_sprites[S_HP_START + i].pc = hp_i < max ? hp_i < cur ? ani_hpfull : ani_hpempty : NULL;
      g_sprites[S_HP_START + i].origin.x = hp_x + (12 - i) * 8;
      g_sprites[S_HP_START + i].origin.y = hp_y;
    }
  }
  next_statdig_spr = S_HP_START + 13;
  place_statnum(cur, max, hp_x, ani_hpnum);
}

static void place_explevelup() {
  i32 i = 0;
  #define ADD(tpc, tx, ty)  do {                 \
      g_sprites[S_EXP_START + i].pc = tpc;       \
      g_sprites[S_EXP_START + i].origin.x = tx;  \
      g_sprites[S_EXP_START + i].origin.y = ty;  \
      i++;                                       \
    } while (0)
  ADD(ani_expiL, 179, 151);
  ADD(ani_expiE, 185, 151);
  ADD(ani_expiV, 192, 151);
  ADD(ani_expiE2, 197, 151);
  ADD(ani_expiL2, 203, 151);
  ADD(ani_expiU, 212, 151);
  ADD(ani_expiP, 218, 151);
  ADD(ani_expiX, 225, 151);
  ADD(ani_expiX2, 228, 151);
  ADD(ani_expiX3, 231, 151);
  ADD(ani_expselect1, 138, 149);
  ADD(ani_expselect2, 138, 149);
  while (i < 25) {
    ADD(NULL, 0, 0);
  }
  #undef ADD
}

static void place_expicons(u8 cur, u8 max) {
  const i32 exp_x = 135;
  const i32 exp_y = 150;
  if (cur < 13) {
    // put empty in the background
    i32 exp_i = 12;
    for (i32 i = 0; i < 13; i++, exp_i--) {
      g_sprites[S_EXP_START + i].pc = exp_i < max ? exp_i < cur ? ani_expfull : ani_expempty : NULL;
      g_sprites[S_EXP_START + i].origin.x = exp_x + (12 - i) * 8;
      g_sprites[S_EXP_START + i].origin.y = exp_y + 2;
    }
    for (i32 i = 13; i < 25; i++, exp_i--) {
      g_sprites[S_EXP_START + i].pc = exp_i < max ? ani_expempty : NULL;
      g_sprites[S_EXP_START + i].origin.x = exp_x + 4 + (24 - i) * 8;
      g_sprites[S_EXP_START + i].origin.y = exp_y;
    }
  } else {
    i32 exp_i = 24;
    for (i32 i = 0; i < 12; i++, exp_i--) {
      g_sprites[S_EXP_START + i].pc = exp_i < max ? exp_i < cur ? ani_expfull : ani_expempty : NULL;
      g_sprites[S_EXP_START + i].origin.x = exp_x + 4 + (11 - i) * 8;
      g_sprites[S_EXP_START + i].origin.y = exp_y + 2;
    }
    for (i32 i = 12; i < 25; i++, exp_i--) {
      g_sprites[S_EXP_START + i].pc = exp_i < max ? exp_i < cur ? ani_expfull : ani_expempty : NULL;
      g_sprites[S_EXP_START + i].origin.x = exp_x + (24 - i) * 8;
      g_sprites[S_EXP_START + i].origin.y = exp_y;
    }
  }
}

static void place_expnum(u8 cur, u8 max) {
  next_statdig_spr = S_EXP_START + 25;
  place_statnum(cur, max, 135, ani_expnum);
}

static const u16 stat_tiles_top[] SECTION_ROM = {
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,
  2,3,4,5,6,7,33,33,33,33,33,33,8,9,10,11,12,13,14,15,16,33,33,33,33,33,33,33,33,33,33,33,
  34,35,36,37,38,39,34,34,34,34,34,34,40,41,42,43,44,45,46,47,48,34,34,34,34,34,34,34,34,34,34,34,
  32,64,65,66,67,68,32,69,70,71,72,73,32,74,75,76,32,32,32,77,78,79,32,32,32,80,81,82,32,32,32,32,
  32,96,97,98,99,100,32,101,102,103,104,105,32,106,107,108,32,32,32,109,110,111,32,32,32,112,113,114,32,32,32,32,
  23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
};
static const u16 stat_tiles_bot[] SECTION_ROM = {
  55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,55,
  55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,55,
  55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,
  55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,55,
  55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,55,
  55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,
  55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,212,213,55,55,
  55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,244,245,55,55,
  55,17,18,19,20,21,22,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,
  55,49,50,51,52,53,54,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,55,
};
static const u16 stat_tiles_bot0[] SECTION_ROM = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static void load_level(u32 seed) {
  struct levelgen_ctx *ctx = malloc(sizeof(struct levelgen_ctx));
  const u8 *levels = BINADDR(levels_bin);
  u32 base = whisky2(seed, 999) & ((BINSIZE(levels_bin) >> 7) - 1);
  levels += base * 128;
  for (i32 i = 0; i < BOARD_SIZE; i++) {
    ctx->board[i] = levels[i];
  }
  levelgen_seed(ctx, seed);
  //levelgen_stage2(ctx); // TODO: uncomment
  game.selx = BOARD_CW;
  game.sely = BOARD_CH;
  for (i32 y = 0; y < BOARD_H; y++) {
    for (i32 x = 0; x < BOARD_W; x++) {
      i32 i = x + y * BOARD_W;
      game.board[i] = ctx->board[i];
      if (GET_TYPE(game.board[i]) == T_LV13) {
        SET_STATUS(game.board[i], 1);
      } else if (GET_TYPE(game.board[i]) == T_ITEM_EYE) {
        game.selx = x;
        game.sely = y;
      }
    }
  }
  free(ctx);
  cursor_to_gamesel();
  cursor_show();

  game.level = 0;
  game.hp = 5;
  game.exp = 0;

  // clear board layers
  sys_set_bgt3_scroll(8, 13);
  sys_set_bgt2_scroll(8, 13);
  for (i32 y = -1; y <= BOARD_H; y++) {
    for (i32 x = -1; x <= BOARD_W; x++) {
      if (x < 0 || x >= BOARD_W || y < 0 || y >= BOARD_H) {
        // place border
        u32 offset = (x + 1) * 4 + (y + 1) * 128;
        sys_set_map(0x1f, offset +  0, 1);
        sys_set_map(0x1f, offset +  2, 1);
        sys_set_map(0x1f, offset + 64, 1);
        sys_set_map(0x1f, offset + 66, 1);
      } else {
        // place background tile
        u8 t = game.board[x + y * BOARD_W];
        switch (GET_STATUS(t)) {
          case 0:
            place_floor(x, y, 0);
            break;
          case 1:
            place_floor(x, y, 0);
            place_answer(x, y, 0);
            break;
          case 2:
            if (GET_TYPE(t) == T_EMPTY) {
              place_floor(x, y, 2);
              place_threat(x, y);
            } else {
              place_floor(x, y, 1);
              place_answer(x, y, IS_ITEM(t) ? 0 : 2);
            }
            break;
        }
      }
    }
  }

  // clear number layer
  sys_set_bgt1_scroll(8, 10);
  for (i32 y = 0; y < BOARD_H; y++) {
    for (i32 x = 0; x < BOARD_W; x++) {
      place_number(x, y, 0, -3);
      place_minehint(x, y, 0);
    }
  }

  // stat layer
  sys_set_bgt0_scroll(4, -144);
  sys_copy_map(0x1c, 0, stat_tiles_top, sizeof(stat_tiles_top));
  sys_copy_map(0x1c, sizeof(stat_tiles_top), stat_tiles_bot0, sizeof(stat_tiles_bot0));
}

static u32 pause_menu() {
  cursor_hide();
  g_statsel_x = 0;
  g_statsel_y = 0;
  // -144 to -104 to -24
  for (i32 i = -140; i <= -24; i += 4) {
    for (i32 j = S_HP_START; j <= S_EXP_END; j++) {
      g_sprites[j].origin.y -= 4;
    }
    nextframe();
    if (i == -104) {
      // TODO: load books, load volumes
      sys_copy_map(0x1c, sizeof(stat_tiles_top), stat_tiles_bot, sizeof(stat_tiles_bot));
    }
    sys_set_bgt0_scroll(4, i);
  }
  cursor_to_statsel();
  cursor_show();
  for (;;) {
    nextframe();
    if (g_hit & SYS_INPUT_U) {
      if (g_statsel_y > 0) {
        g_statsel_y--;
        cursor_to_statsel();
      } else {
        sfx_bump();
      }
    } else if (g_hit & SYS_INPUT_R) {
      if (g_statsel_y == 0 && g_statsel_x < 8) {
        g_statsel_x += 2;
        cursor_to_statsel();
      } else if (g_statsel_y > 0 && g_statsel_x < 9) {
        g_statsel_x++;
        cursor_to_statsel();
      } else {
        sfx_bump();
      }
    } else if (g_hit & SYS_INPUT_D) {
      if (g_statsel_y == 0) {
        g_statsel_y = 1;
        cursor_to_statsel();
      } else if (g_statsel_y < 3) {
        g_statsel_y++;
        cursor_to_statsel();
      } else {
        sfx_bump();
      }
    } else if (g_hit & SYS_INPUT_L) {
      if (g_statsel_y == 0 && g_statsel_x > 1) {
        g_statsel_x -= 2;
        cursor_to_statsel();
      } else if (g_statsel_y > 0 && g_statsel_x > 0) {
        g_statsel_x--;
        cursor_to_statsel();
      } else {
        sfx_bump();
      }
    } else if (g_hit & SYS_INPUT_ST) {
      break;
    }
  }
  cursor_hide();
  for (i32 i = -28; i >= -144; i -= 4) {
    for (i32 j = S_HP_START; j <= S_EXP_END; j++) {
      g_sprites[j].origin.y += 4;
    }
    nextframe();
    if (i == -104) {
      sys_copy_map(0x1c, sizeof(stat_tiles_top), stat_tiles_bot0, sizeof(stat_tiles_bot0));
    }
    sys_set_bgt0_scroll(4, i);
  }
  cursor_to_gamesel();
  cursor_show();
}

void gvmain() {
  sys_init();
  game.brightness = 2;
  game.songvol = 6;
  game.sfxvol = 16;
  snd_set_master_volume(16);
  snd_set_song_volume(game.songvol);
  snd_set_sfx_volume(game.sfxvol);
  snd_load_song(BINADDR(song1_gvsong), 1);
  sys_set_vblank(irq_vblank);
  gfx_setmode(GFX_MODE_4T);
  gfx_showbg0(true); // UI/pause
  sys_set_bg_config(
    0, // background #
    0, // priority
    2, // tile start
    0, // mosaic
    1, // 256 colors
    0x1c, // map start
    0, // wrap
    SYS_BGT_SIZE_256X256
  );
  gfx_showbg1(true); // numbers/marks
  sys_set_bg_config(
    1, // background #
    0, // priority
    0, // tile start
    0, // mosaic
    1, // 256 colors
    0x1d, // map start
    0, // wrap
    SYS_BGT_SIZE_256X256
  );
  gfx_showbg2(true); // board fg
  sys_set_bg_config(
    2, // background #
    0, // priority
    0, // tile start
    0, // mosaic
    1, // 256 colors
    0x1e, // map start
    0, // wrap
    SYS_BGT_SIZE_256X256
  );
  gfx_showbg3(true); // board bg
  sys_set_bg_config(
    3, // background #
    0, // priority
    0, // tile start
    0, // mosaic
    1, // 256 colors
    0x1f, // map start
    0, // wrap
    SYS_BGT_SIZE_256X256
  );
  gfx_showobj(true);
  sys_copy_tiles(0, 0, BINADDR(tiles_bin), BINSIZE(tiles_bin));
  sys_copy_tiles(2, 0, BINADDR(ui_bin), BINSIZE(ui_bin));
  sys_copy_tiles(4, 0, BINADDR(sprites_bin), BINSIZE(sprites_bin));
  sys_copy_bgpal(0, BINADDR(palette_brightness_bin) + game.brightness * 512, 512);
  sys_copy_spritepal(0, BINADDR(palette_brightness_bin) + game.brightness * 512, 512);
  gfx_showscreen(true);

  load_level(1234);
  place_hp(12, 13);
  place_expnum(25, 25);
  place_expicons(25, 25);
  place_explevelup();

  for (;;) {
    nextframe();
    if (g_hit & SYS_INPUT_U) {
      if (game.sely > 0) {
        game.sely--;
      } else {
        game.sely = BOARD_H - 1;
      }
      cursor_to_gamesel();
    } else if (g_hit & SYS_INPUT_R) {
      if (game.selx < BOARD_W - 1) {
        game.selx++;
      } else {
        game.selx = 0;
      }
      cursor_to_gamesel();
    } else if (g_hit & SYS_INPUT_D) {
      if (game.sely < BOARD_H - 1) {
        game.sely++;
      } else {
        game.sely = 0;
      }
      cursor_to_gamesel();
    } else if (g_hit & SYS_INPUT_L) {
      if (game.selx > 0) {
        game.selx--;
      } else {
        game.selx = BOARD_W - 1;
      }
      cursor_to_gamesel();
    } else if (g_hit & SYS_INPUT_ST) {
      pause_menu();
    } else if (g_hit & SYS_INPUT_B) {
      // TODO: marker popup
    } else if (g_hit & SYS_INPUT_A) {
      u32 k = game.selx + game.sely * BOARD_W;
      if (GET_STATUS(game.board[k]) == 2) {
        // collect
        switch (GET_TYPE(game.board[k])) {
          case T_EMPTY: break;
          case T_LV1A:
          case T_LV1B:
          case T_LV2:
          case T_LV3:
          case T_LV4:
          case T_LV5A:
          case T_LV5B:
          case T_LV5C:
          case T_LV6:
          case T_LV7A:
          case T_LV7B:
          case T_LV7C:
          case T_LV7D:
          case T_LV8:
          case T_LV9:
          case T_LV10:
          case T_LV11:
          case T_LV13:
            // TODO: fight monster
            break;
          case T_MINE:
            // TODO: die
            break;
          case T_WALL:
            // TODO: attack wall
            break;
          case T_CHEST_HEAL:
            SET_TYPE(game.board[k], T_ITEM_HEAL);
            place_answer(game.selx, game.sely, 0);
            place_floor(game.selx, game.sely, 1);
            break;
          case T_CHEST_EYE:
            SET_TYPE(game.board[k], T_ITEM_EYE);
            place_answer(game.selx, game.sely, 0);
            place_floor(game.selx, game.sely, 1);
            break;
          case T_CHEST_EXP:
            SET_TYPE(game.board[k], T_ITEM_EXP5);
            place_answer(game.selx, game.sely, 0);
            place_floor(game.selx, game.sely, 1);
            break;
        }
      } else {
        // attack
        SET_STATUS(game.board[k], 2);
        switch (GET_TYPE(game.board[k])) {
          case T_EMPTY:
            place_floor(game.selx, game.sely, 2);
            place_threat(game.selx, game.sely);
            break;
          case T_LV1A:
          case T_LV1B:
          case T_LV2:
          case T_LV3:
          case T_LV4:
          case T_LV5A:
          case T_LV5B:
          case T_LV5C:
          case T_LV6:
          case T_LV7A:
          case T_LV7B:
          case T_LV7C:
          case T_LV7D:
          case T_LV8:
          case T_LV9:
          case T_LV10:
          case T_LV11:
          case T_LV13:
            // TODO: fight monster
            place_floor(game.selx, game.sely, 1);
            place_answer(game.selx, game.sely, 2);
            break;
          case T_MINE:
            // TODO: die
            place_floor(game.selx, game.sely, 2);
            place_answer(game.selx, game.sely, 2);
            break;
          case T_WALL:
          case T_CHEST_HEAL:
          case T_CHEST_EYE:
          case T_CHEST_EXP:
            place_answer(game.selx, game.sely, 0);
            break;
          case T_ITEM_HEAL:
          case T_ITEM_EYE:
          case T_ITEM_SHOW1:
          case T_ITEM_SHOW5:
          case T_ITEM_EXP1:
          case T_ITEM_EXP3:
          case T_ITEM_EXP5:
            place_floor(game.selx, game.sely, 2);
            place_answer(game.selx, game.sely, 0);
            break;
        }
      }
    }
  }
}

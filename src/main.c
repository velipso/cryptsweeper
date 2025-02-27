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
#include "rnd.h"
#include "game.h"

#define S_POPUPCUR     0
#define S_POPUP        1
// title sprites (can overlap with game sprites)
#define S_TITLE_START  2
#define S_TITLE_END    20
// game sprites
#define S_CURSOR1      2
#define S_CURSOR2      3
#define S_CURSOR3      4
#define S_CURSOR4      5
#define S_HP_START     6
#define S_HP_END       24
#define S_EXP_START    25
#define S_EXP_END      54
#define S_PART_START   55
#define S_PART_END     123
#define S_KING1_BODY   124
#define S_KING2_BODY   125
#define S_KING3_BODY   126
#define S_KING4_BODY   127

enum book_enum {
  B_INTRO,
  B_ITEMS,
  B_LOWLEVEL,
  B_LV1B,
  B_LV3B,
  B_LV3C,
  B_LV4A,
  B_LV4B,
  B_LV4C,
  B_LV5A,
  B_LV5B,
  B_LV5C,
  B_LV6,
  B_LV7,
  B_LV8,
  B_LV9,
  B_LV10,
  B_LV11,
  B_LV13,
  B_MINE,
  B_WALL,
  B_CHEST,
  B_LAVA,
  B_EASY,
  B_MILD,
  B_NORMAL,
  B_HARD,
  B_EXPERT,
  B_ONLYMINES,
  B_100
};
#define B__SIZE      (B_100 + 1)
#define RESET_BOOKS  0

enum book_info_action {
  BI_DRAW,
  BI_CHECK,
  BI_CLICK
};

struct save_st {
  u32 checksum;
  u32 books;
  u8 songvol;
  u8 sfxvol;
  u8 brightness;
  u8 reserved;
  struct game_st game;
};

static u32 g_down;
static u32 g_hit;
static i32 g_cursor_x;
static i32 g_cursor_y;
static i32 g_statsel_x;
static i32 g_statsel_y;
static i32 g_next_particle = S_PART_START;
static bool g_peek = false;
static bool g_cheat = true; // TODO: change to false on release

struct save_st saveroot;
static struct game_st *const game = &saveroot.game;
struct save_st savecopy SECTION_EWRAM;
u8 *const saveaddr = (u8 *)0x0e000000;
struct rnd_st g_rnd = { 1, 1 };
static bool g_showing_levelup;
static const struct {
  const i32 t;
  const u16 *const ani_alive;
  const u16 *const ani_dead;
} g_kinginfo[4] = {
  { T_LV1B, ani_lv1b, ani_lv1b_dead },
  { T_LV5B, ani_lv5b, ani_lv5b_dead },
  { T_LV10, ani_lv10, ani_lv10_dead },
  { T_LV13, ani_lv13, ani_lv13_dead }
};
static struct { i32 x, y; } g_kingxy[4];

// map 0-10 -> 0-16
static const u16 volume_map_fwd[] SECTION_ROM =
  {0, 1, 2, 3, 5, 6, 8, 10, 12, 14, 16, 16};
// map 0-16 -> 0-10
static const u16 volume_map_back[] SECTION_ROM =
  {0, 1, 2, 3, 3, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10};

static void handler(struct game_st *game, enum game_event ev, i32 x, i32 y);
static void book_award(enum book_enum book, i32 return_mode);
static void book_click(enum book_enum book, i32 return_mode);
static i32 book_info(enum book_enum book, enum book_info_action action);
static void palette_fadetoblack();
static void palette_fadefromblack();

static u32 g_shake = 0;
static u32 g_shake_frame;
static void shake_screen() {
  g_shake = 6;
  g_shake_frame = 0;
}
static void SECTION_IWRAM_ARM irq_vblank() {
  u32 inp = sys_input() ^ 0x3ff;
  g_hit = ~g_down & inp;
  g_down = inp;
  g_rnd.seed = whisky2(g_rnd.seed, inp);
  if (g_shake) {
    g_shake_frame++;
    if (g_shake_frame >= 4) {
      g_shake--;
      g_shake_frame = 0;
      i32 amt = 0;
      if (g_shake) {
        amt += (g_rnd.seed & 3) + 1;
        if (g_shake & 1) amt = -amt;
      }
      // shake kings
      for (i32 king = 0; king < 4; king++) {
        if (g_sprites[S_KING1_BODY + king].pc) {
          g_sprites[S_KING1_BODY + king].origin.x = g_kingxy[king].x * 16 + 8 - amt;
          ani_flushxy(S_KING1_BODY + king);
        }
      }
      sys_set_bgt3_scroll(8 + amt, 13);
      sys_set_bgt2_scroll(8 + amt, 13);
      sys_set_bgt1_scroll(8 + amt, 10);
    }
  }
  sys_copy_oam(g_oam);
}

static u32 calculate_checksum(struct save_st *g) {
  u32 old_checksum = g->checksum;
  g->checksum = 0;
  u32 checksum = 123;
  const u8 *d = (const u8 *)g;
  for (i32 i = 0; i < sizeof(struct save_st); i++, d++) {
    checksum = whisky2(checksum, *d);
  }
  g->checksum = old_checksum;
  return checksum;
}

static void load_savecopy() {
  memcpy8(&savecopy, saveaddr, sizeof(struct save_st));
}

static inline void save_savecopy(bool del) {
  if (!del) {
    memcpy8(&savecopy, &saveroot, sizeof(struct save_st));
  }
  savecopy.checksum = calculate_checksum(&savecopy);
  if (del) {
    // if deleting, corrupt the checksum
    savecopy.checksum ^= 0xaa55a5a5;
  }
  memcpy8(saveaddr, &savecopy, sizeof(struct save_st));
}

static void nextframe() {
  for (int i = 0; i < 128; i++)
    ani_step(i);
  sys_nextframe();
}

static void waitstart() {
  i32 delay = 30;
  for (;;) {
    nextframe();
    if (delay > 0) delay--;
    else if ((g_hit & SYS_INPUT_ST) || (g_hit && SYS_INPUT_A)) break;
  }
}

static u32 dig99(i32 num) {
  if (num < 0) num = 0;
  if (num > 99) num = 99;
  u32 d1 =
    num < 10 ? 0 :
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

static void place_particle(i32 x, i32 y, i32 dx, i32 dy, i32 ddy, const u16 *spr) {
  i32 i = g_next_particle;
  g_next_particle++;
  if (g_next_particle > S_PART_END) {
    g_next_particle = S_PART_START;
  }
  g_sprites[i].pc = spr;
  g_sprites[i].origin.x = x;
  g_sprites[i].origin.y = y;
  g_sprites[i].offset.dx = dx;
  g_sprites[i].offset.dy = dy;
  g_sprites[i].gravity = ddy;
}

static void place_particles_press(i32 x, i32 y) {
  x = x * 16 + 8;
  y = y * 16 + 3;
  for (i32 i = 0; i < 15; i++) {
    i32 dx = roll(&g_rnd, 16);
    place_particle(
      x + dx,
      y + roll(&g_rnd, 16),
      (dx < 8 ? -1 : 1) * (roll(&g_rnd, 0x0040) + 0x0020),
      -0x0080,
      0x0008,
      roll(&g_rnd, 2) ? ani_gray1 : ani_gray2
    );
  }
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

static void clear_answer(i32 x, i32 y) {
  u32 offset = (x + 1) * 4 + (y + 1) * 128;
  sys_set_map(0x1e, offset +  0, 0);
  sys_set_map(0x1e, offset +  2, 0);
  sys_set_map(0x1e, offset + 64, 0);
  sys_set_map(0x1e, offset + 66, 0);
}

static void place_answer(i32 x, i32 y, i32 frame) {
  frame *= 2;
  i32 t = game_tileicon(game->board[x + y * BOARD_W]);
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

static void place_lava(i32 x, i32 y) {
  // construct bitmask for the 8 directions
  i32 mask = 0;
  for (i32 dy = -1, i = 0; dy <= 1; dy++) {
    i32 by = y + dy;
    for (i32 dx = -1; dx <= 1; dx++, i++) {
      i32 bx = x + dx;
      if (
        bx >= 0 && bx < BOARD_W &&
        by >= 0 && by < BOARD_H &&
        GET_TYPEXY(game->board, bx, by) == T_LAVA
      ) {
        mask |= 1 << i;
      }
    }
  }

  u32 offset = (x + 1) * 4 + (y + 1) * 128;

  // mask:
  //  +---+---+---+
  //  | 1 | 2 | 4 |
  //  +---+---+---+
  //  | 8 | 16| 32|  (center at 16 is always lava)
  //  +---+---+---+
  //  | 64|128|256|
  //  +---+---+---+

  // pick appropriate frames based on if lava is around this tile
  int ul = 0, ur = 0, dl = 0, dr = 0;

  if (mask &   2) { ul += 2; ur += 2; } // if up
  if (mask & 128) { dl += 2; dr += 2; } // if down
  if (mask &   8) { ul += 4; dl += 4; } // if left
  if (mask &  32) { ur += 4; dr += 4; } // if right

  // check for completely full
  if ((mask & ( 1+  2+  8)) ==  1+  2+  8) ul += 2;
  if ((mask & ( 2+  4+ 32)) ==  2+  4+ 32) ur += 2;
  if ((mask & ( 8+ 64+128)) ==  8+ 64+128) dl += 2;
  if ((mask & (32+128+256)) == 32+128+256) dr += 2;

  sys_set_map(0x1f, offset +  0, 8 + ul);
  sys_set_map(0x1f, offset +  2, 9 + ur);
  sys_set_map(0x1f, offset + 64, 40 + dl);
  sys_set_map(0x1f, offset + 66, 41 + dr);
}

static void place_floor(i32 x, i32 y, i32 state) {
  if (GET_TYPEXY(game->board, x, y) == T_LAVA) {
    place_lava(x, y);
    return;
  }
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

static void place_threat(i32 x, i32 y) {
  if (x < 0 || x >= BOARD_W || y < 0 || y >= BOARD_H) return;
  i32 threat = count_threat(game->board, x, y);
  if (game->difficulty & D_ONLYMINES) {
    threat >>= 8;
    place_number(x, y, 0, threat == 0 ? -3 : threat);
  } else {
    place_minehint(x, y, threat >> 8);
    if ((threat & 0xff) == 0xff) { // hidden threat
      place_number(x, y, 0, -1);
    } else if (threat) {
      place_number(x, y, 0, threat & 0xff);
    } else {
      place_number(x, y, 0, -3);
    }
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

static void cursor_pause() {
  g_sprites[S_CURSOR1].pc = ani_cursor1_pause;
  g_sprites[S_CURSOR2].pc = ani_cursor2_pause;
  g_sprites[S_CURSOR3].pc = ani_cursor3_pause;
  g_sprites[S_CURSOR4].pc = ani_cursor4_pause;
}

static void cursor_to_gamesel_offset(i32 dx, i32 dy) {
  g_cursor_x = game->selx * 16 + 8 + dx;
  g_cursor_y = game->sely * 16 + 3 + dy;
  g_sprites[S_CURSOR1].origin.x = g_cursor_x;
  g_sprites[S_CURSOR1].origin.y = g_cursor_y;
  g_sprites[S_CURSOR2].origin.x = g_cursor_x + 16;
  g_sprites[S_CURSOR2].origin.y = g_cursor_y;
  g_sprites[S_CURSOR3].origin.x = g_cursor_x;
  g_sprites[S_CURSOR3].origin.y = g_cursor_y + 16;
  g_sprites[S_CURSOR4].origin.x = g_cursor_x + 16;
  g_sprites[S_CURSOR4].origin.y = g_cursor_y + 16;
}

static void cursor_to_gamesel() {
  cursor_to_gamesel_offset(0, 0);
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

static void place_statnum(u8 cur, u8 max, i32 x, const u16 *pc[], i32 end_spr) {
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
  // clear out remaining sprites
  while (next_statdig_spr <= end_spr) {
    g_sprites[next_statdig_spr++].pc = NULL;
  }
}

static void place_explevelup() {
  g_showing_levelup = true;
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
  g_showing_levelup = false;
  const i32 exp_x = 135;
  const i32 exp_y = 149;
  if (cur < 13) {
    // put empty in the background
    i32 exp_i = 12;
    for (i32 i = 0; i < 13; i++, exp_i--) {
      g_sprites[S_EXP_START + i].pc = exp_i < max ? exp_i < cur ? ani_expfull : ani_expempty : NULL;
      g_sprites[S_EXP_START + i].origin.x = exp_x + (12 - i) * 8;
      g_sprites[S_EXP_START + i].origin.y = exp_y + 2;
    }
    exp_i = 24;
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
  place_statnum(cur, max, 135, ani_expnum, S_EXP_END);
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

static void load_level(i32 diff, u32 seed) {
  u32 group = seed & (GENERATE_SIZE - 1);
  sys_print("new game seed: %x, group: %x, diff: %x", seed, group, diff);
  const u8 *levels = BINADDR(levels_bin);
  levels += group * 128 * 6;
  if (diff & D_ONLYMINES) {
    game_new(game, diff, seed, levels + 128 * 5);
  } else {
    game_new(game, diff, seed, levels + 128 * diff);
  }
}

static void load_scr_raw(const void *addr, u32 size, bool showobj) {
  gfx_setmode(GFX_MODE_2I);
  gfx_showbg0(false);
  gfx_showbg1(false);
  sys_set_bgs2_scroll(0, 0);
  gfx_showbg2(true);
  gfx_showbg3(false);
  gfx_showobj(showobj);
  sys_copy_tiles(0, 0, addr, size);
}
#define load_scr(a) load_scr_raw(BINADDR(a), BINSIZE(a), true)

static void book_scr_raw(const void *addr, u32 size) {
  load_scr_raw(addr, size, false);
  palette_fadefromblack();
  waitstart();
  palette_fadetoblack();
}
#define book_scr(a) book_scr_raw(BINADDR(a), BINSIZE(a))

static void tile_update(i32 x, i32 y) {
  if (x < 0 || x >= BOARD_W || y < 0 || y >= BOARD_H) return;
  i32 k = x + y * BOARD_W;
  u8 t = game->board[k];

  for (i32 king = 0; king < 4; king++) {
    if (x == g_kingxy[king].x && y == g_kingxy[king].y) {
      if (
        GET_TYPE(t) != g_kinginfo[king].t ||
        (!g_peek && !game->win && GET_STATUS(t) == S_HIDDEN)
      ) {
        g_sprites[S_KING1_BODY + king].pc = NULL;
      } else {
        if (GET_STATUS(t) == S_PRESSED) {
          g_sprites[S_KING1_BODY + king].pc = g_kinginfo[king].ani_dead;
        } else {
          g_sprites[S_KING1_BODY + king].pc = g_kinginfo[king].ani_alive;
        }
        g_sprites[S_KING1_BODY + king].origin.x = g_kingxy[king].x * 16 + 8;
        g_sprites[S_KING1_BODY + king].origin.y = g_kingxy[king].y * 16 + 3;
      }
    }
  }

  i32 frame = IS_MONSTER(t) && GET_TYPE(t) != T_LV11 ? (rnd32(&g_rnd) & 1) : 0;
  switch (GET_STATUS(t)) {
    case S_HIDDEN:
      place_floor(x, y, 0);
      if (game->win) {
        place_answer(x, y, frame);
        place_number(x, y, 0, -3);
      } else {
        if (g_peek) {
          if (GET_TYPE(t) == T_LV11) frame = 1;
          place_answer(x, y, frame);
        } else {
          clear_answer(x, y);
        }
        place_number(x, y, 4, game->notes[k]);
      }
      place_minehint(x, y, 0);
      break;
    case S_VISIBLE:
      place_floor(x, y, 0);
      place_answer(x, y, frame);
      if (game->difficulty & D_ONLYMINES) {
        place_number(x, y, 4, game->notes[k]);
      } else {
        place_number(x, y, 0, -3);
      }
      place_minehint(x, y, 0);
      break;
    case S_PRESSED:
      if (IS_EMPTY(t)) {
        place_floor(x, y, 2);
        place_threat(x, y);
        clear_answer(x, y);
      } else if (GET_TYPE(t) == T_WALL || IS_CHEST(t)) {
        place_floor(x, y, 0);
        place_answer(x, y, 0);
        place_number(x, y, 0, -3);
        place_minehint(x, y, 0);
      } else {
        if (game->win == 2 && GET_TYPE(t) == T_ITEM_EXIT) {
          place_floor(x, y, 2);
        } else {
          place_floor(x, y, 1);
        }
        place_answer(x, y, IS_ITEM(t) ? 0 : 2);
        place_number(x, y, 0, -3);
        place_minehint(x, y, 0);
      }
      break;
    case S_KILLED:
      place_floor(x, y, 2);
      if (GET_TYPE(t) == T_LV11) frame = 1;
      else if (GET_TYPE(t) == T_MINE) frame = 2;
      place_answer(x, y, frame);
      if (IS_EMPTY(t)) {
        place_threat(x, y);
      } else {
        place_number(x, y, 0, -3);
        place_minehint(x, y, 0);
      }
      break;
  }
}

static void set_peek(bool f) {
  g_peek = f;
  for (i32 y = 0; y < BOARD_H; y++) {
    for (i32 x = 0; x < BOARD_W; x++) {
      tile_update(x, y);
    }
  }
}

static void hp_update(i32 cur, i32 max) {
  const i32 hp_x = 37;
  const i32 hp_y = 149;
  #define HP_SPRITE  hp_i >= max ? NULL : hp_i < cur ? ani_hpfull : ani_hpempty
  if (cur <= 7) {
    // put empty in the background
    i32 hp_i = 6;
    for (i32 i = 0; i < 7; i++, hp_i--) {
      g_sprites[S_HP_START + i].pc = HP_SPRITE;
      g_sprites[S_HP_START + i].origin.x = hp_x + (6 - i) * 8;
      g_sprites[S_HP_START + i].origin.y = hp_y + 2;
    }
    hp_i = 12;
    for (i32 i = 7; i < 13; i++, hp_i--) {
      g_sprites[S_HP_START + i].pc = HP_SPRITE;
      g_sprites[S_HP_START + i].origin.x = hp_x + 4 + (12 - i) * 8;
      g_sprites[S_HP_START + i].origin.y = hp_y;
    }
  } else {
    i32 hp_i = 12;
    for (i32 i = 0; i < 6; i++, hp_i--) {
      g_sprites[S_HP_START + i].pc = HP_SPRITE;
      g_sprites[S_HP_START + i].origin.x = hp_x + 4 + (5 - i) * 8;
      g_sprites[S_HP_START + i].origin.y = hp_y + 2;
    }
    for (i32 i = 6; i < 13; i++, hp_i--) {
      g_sprites[S_HP_START + i].pc = HP_SPRITE;
      g_sprites[S_HP_START + i].origin.x = hp_x + (12 - i) * 8;
      g_sprites[S_HP_START + i].origin.y = hp_y;
    }
  }
  g_sprites[S_HP_START + 13].pc = next_level_hp_increases(game) ? ani_hphalf : NULL;
  g_sprites[S_HP_START + 13].origin.x = hp_x + (max > 7 ? 7 : max) * 8;
  g_sprites[S_HP_START + 13].origin.y = hp_y + 2;
  next_statdig_spr = S_HP_START + 14;
  place_statnum(cur, max, hp_x, ani_hpnum, S_HP_END);
  #undef HP_SPRITE
}

static void exp_update(i32 cur, i32 max) {
  place_expnum(cur, max);
  if (!game->win && cur >= max) {
    if (!g_showing_levelup) {
      place_explevelup();
    }
  } else {
    place_expicons(cur, max);
  }
}

static void you_lose(i32 x, i32 y) {
  cursor_hide();
  if (GET_TYPE(game->board[x + y * BOARD_W]) == T_MINE) {
    shake_screen();
  }
  save_savecopy(false);
}

static void draw_level();
static void you_win() {
  cursor_hide();
  palette_fadetoblack();
  save_savecopy(false);
  for (i32 king = 0; king < 4; king++) {
    g_sprites[S_KING1_BODY + king].pc = NULL;
  }
  if (game->difficulty & D_ONLYMINES) {
    load_scr(scr_winmine_o);
  } else {
    // TODO: different end screens
    load_scr(scr_wingame_o);
  }
  palette_fadefromblack();
  waitstart();
  palette_fadetoblack();
  draw_level();
  palette_fadefromblack();

  i32 new_book = -1;
  i32 avail_books = 0;
  for (i32 b = 0; b < B__SIZE; b++) {
    if (saveroot.books & (1 << b)) continue; // skips books we already have
    i32 pts = book_info(b, BI_CHECK);
    if (pts >= 999) {
      // force this book
      new_book = b;
      avail_books = -1;
      break;
    }
    for (; pts > 0; pts--) {
      if (rnd_pick(&g_rnd, avail_books)) new_book = b;
      avail_books++;
    }
  }
  if (
    (avail_books < 0 || roll(&g_rnd, 2)) && // 50% chance, unless forced
    new_book >= 0
  ) {
    book_award(new_book, 2);
    if (saveroot.books == (((1 << B__SIZE) - 1) & ~(1 << B_100))) {
      // got 100%!
      book_award(B_100, 2);
    }
  }
}

static void handler(struct game_st *game, enum game_event ev, i32 x, i32 y) {
  switch (ev) {
    case EV_PRESS_EMPTY: place_particles_press(x, y); break;
    case EV_TILE_UPDATE: tile_update(x, y); break;
    case EV_HP_UPDATE:   hp_update(x, y);   break;
    case EV_EXP_UPDATE:  exp_update(x, y);  break;
    case EV_SHOW_LAVA:   shake_screen();    break;
    case EV_HOVER_LAVA:  shake_screen();    break;
    case EV_YOU_LOSE:    you_lose(x, y);    break;
    case EV_YOU_WIN:     you_win();         break;
    case EV_WAIT:        for (i32 i = 0; i < x; i++) nextframe(); break;
    case EV_DEBUGLOG:    sys_print("[%x] value %x", x, y); break;
  }
}

static void set_game_gfx() {
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
    1, // priority
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
    1, // priority
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
    1, // priority
    0, // tile start
    0, // mosaic
    1, // 256 colors
    0x1f, // map start
    0, // wrap
    SYS_BGT_SIZE_256X256
  );
  gfx_showobj(true);
}

static void draw_level() {
  set_game_gfx();
  sys_copy_tiles(0, 0, BINADDR(tiles_bin), BINSIZE(tiles_bin));
  sys_copy_tiles(2, 0, BINADDR(ui_bin), BINSIZE(ui_bin));

  if (game->win) {
    cursor_hide();
  } else {
    cursor_to_gamesel();
    cursor_show();
  }

  // clear board layers
  sys_set_bgt3_scroll(8, 13);
  sys_set_bgt2_scroll(8, 13);
  sys_set_bgt1_scroll(8, 10);
  for (i32 king = 0; king < 4; king++) {
    g_kingxy[king].x = -1;
    g_sprites[S_KING1_BODY + king].pc = NULL;
  }
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
        for (i32 king = 0; king < 4; king++) {
          if (GET_TYPEXY(game->board, x, y) == g_kinginfo[king].t) {
            g_kingxy[king].x = x;
            g_kingxy[king].y = y;
          }
        }
        tile_update(x, y);
      }
    }
  }

  // stat layer
  sys_set_bgt0_scroll(4, -144);
  sys_copy_map(0x1c, 0, stat_tiles_top, sizeof(stat_tiles_top));
  sys_copy_map(0x1c, sizeof(stat_tiles_top), stat_tiles_bot0, sizeof(stat_tiles_bot0));
  if (game->difficulty & D_ONLYMINES) {
    // remove hp/exp display entirely
    for (i32 i = 0; i < 128; i += 2) {
      sys_set_map(0x1c, 128 + i, i < 64 ? 33 : 34);
    }
    for (i32 i = S_HP_START; i <= S_EXP_END; i++) {
      g_sprites[i].pc = NULL;
    }
  } else {
    hp_update(game->hp, max_hp(game));
    exp_update(game->exp, max_exp(game));
  }
}

static void draw_books() {
  for (i32 b = 0; b < B__SIZE; b++) {
    if (saveroot.books & (1 << b)) {
      i32 bi = book_info(b, BI_DRAW);
      i32 x = bi & 0xf;
      i32 y = (bi >> 4) & 0xf;
      i32 color = (bi >> 8) & 0xf;
      u32 offset = 450 + x * 6 + y * 192;
      u32 t = x * 2 + (y + 3) * 64;
      color *= 3;
      u32 yb = y == 0 ? 87 : 119;
      sys_set_map(0x1c, offset - 64, yb + color);
      sys_set_map(0x1c, offset - 62, yb + 1 + color);
      sys_set_map(0x1c, offset - 60, yb + 2 + color);
      sys_set_map(0x1c, offset + 4, 153 + color);
      sys_set_map(0x1c, offset + 68, 153 + 32 + color);
      sys_set_map(0x1c, offset +  0, t);
      sys_set_map(0x1c, offset +  2, t + 1);
      sys_set_map(0x1c, offset + 64, t + 32);
      sys_set_map(0x1c, offset + 66, t + 33);
    }
  }
}

static void popup_show(i32 popup_index, i32 height) {
  height &= ~7;
  const void *popup_addr = BINADDR(popups_bin);
  popup_addr += 8 * 512 * popup_index;
  for (i32 i = 0; i < 8; i++, popup_addr += 512) {
    sys_copy_tiles(4, 16384 + i * 1024, popup_addr, 512);
  }
  g_sprites[S_POPUP].pc = ani_popup;
  g_sprites[S_POPUP].origin.x = 88;
  for (i32 i = -64; i <= height; i += 8) {
    g_sprites[S_POPUP].origin.y = i;
    nextframe();
  }
}

static void popup_hide(i32 height) {
  height &= ~7;
  for (i32 i = height; i >= -64; i -= 8) {
    g_sprites[S_POPUP].origin.y = i;
    nextframe();
  }
  g_sprites[S_POPUP].pc = NULL;
}

static i32 popup_delete() {
  popup_show(30, 72);
  g_sprites[S_POPUPCUR].pc = ani_arrowr2;
  g_sprites[S_POPUPCUR].origin.x = 97;
  i32 menu = 0;
  for (;;) {
    g_sprites[S_POPUPCUR].origin.y = 90 + menu * 9;
    nextframe();
    if (g_hit & SYS_INPUT_U) {
      if (menu > 0) {
        menu--;
      }
    } else if (g_hit & SYS_INPUT_D) {
      if (menu < 1) {
        menu++;
      }
    } else if ((g_hit & SYS_INPUT_A) || (g_hit & SYS_INPUT_ST)) {
      break;
    } else if (g_hit & SYS_INPUT_B) {
      menu = 0;
      break;
    }
  }
  g_sprites[S_POPUPCUR].pc = NULL;
  popup_hide(72);
  return menu;
}

static void popup_cheat() {
  popup_show(32, 72);
  g_sprites[S_POPUPCUR].pc = ani_arrowr2;
  g_sprites[S_POPUPCUR].origin.x = 97;
  i32 menu = g_cheat ? 1 : 0;
  for (;;) {
    g_sprites[S_POPUPCUR].origin.y = 83 + menu * 9;
    nextframe();
    if (g_hit & SYS_INPUT_U) {
      if (menu > 0) {
        menu--;
      }
    } else if (g_hit & SYS_INPUT_D) {
      if (menu < 1) {
        menu++;
      }
    } else if ((g_hit & SYS_INPUT_A) || (g_hit & SYS_INPUT_ST)) {
      break;
    } else if (g_hit & SYS_INPUT_B) {
      menu = 0;
      break;
    }
  }
  g_sprites[S_POPUPCUR].pc = NULL;
  popup_hide(72);
  g_cheat = menu == 1;
  set_peek(g_cheat ? g_peek : false);
}

static i32 popup_newgame() {
  popup_show(31, 40);
  g_sprites[S_POPUPCUR].pc = ani_arrowr2;
  g_sprites[S_POPUPCUR].origin.x = 97;
  i32 difficulty = game->difficulty & D_DIFFICULTY;
  for (;;) {
    g_sprites[S_POPUPCUR].origin.y = 51 + difficulty * 9;
    nextframe();
    if (g_hit & SYS_INPUT_U) {
      if (difficulty > 0) {
        difficulty--;
      }
    } else if (g_hit & SYS_INPUT_D) {
      if (difficulty < 4) {
        difficulty++;
      }
    } else if ((g_hit & SYS_INPUT_A) || (g_hit & SYS_INPUT_ST)) {
      if ((g_down & SYS_INPUT_ZL) && (g_down & SYS_INPUT_ZR)) {
        difficulty |= D_ONLYMINES;
      }
      break;
    } else if (g_hit & SYS_INPUT_B) {
      difficulty = -1;
      break;
    }
  }
  g_sprites[S_POPUPCUR].pc = NULL;
  popup_hide(40);
  return difficulty;
}

static void set_option_tiles() {
  #define SETNUM(offset, value) do {            \
      u16 v = 128 + (value) * 2;                \
      sys_set_map(0x1c, 288 + offset, v +  0);  \
      sys_set_map(0x1c, 290 + offset, v +  1);  \
      sys_set_map(0x1c, 352 + offset, v + 32);  \
      sys_set_map(0x1c, 354 + offset, v + 33);  \
    } while (0)
  SETNUM(0, volume_map_back[saveroot.songvol]);
  SETNUM(12, volume_map_back[saveroot.sfxvol]);
  SETNUM(24, saveroot.brightness + 1);
  #undef SETNUM
}

static void palette_black(i32 amt) {
  u16 *pal = malloc(512);
  const u16 *inp = BINADDR(palette_brightness_bin) + saveroot.brightness * 512;
  for (i32 i = 0; i < 256; i++) {
    u16 c = inp[i];
    i32 r = c & 0x1f;
    i32 g = (c >> 5) & 0x1f;
    i32 b = (c >> 10) & 0x1f;
    r = (r * amt) >> 3;
    g = (g * amt) >> 3;
    b = (b * amt) >> 3;
    pal[i] = r | (g << 5) | (b << 10);
  }
  sys_copy_bgpal(0, pal, 512);
  sys_copy_spritepal(0, pal, 512);
  free(pal);
}

static void palette_white(i32 amt) {
  u16 *pal = malloc(512);
  const u16 *inp = BINADDR(palette_brightness_bin) + saveroot.brightness * 512;
  for (i32 i = 0; i < 256; i++) {
    u16 c = inp[i];
    i32 r = c & 0x1f;
    i32 g = (c >> 5) & 0x1f;
    i32 b = (c >> 10) & 0x1f;
    r = 31 - r;
    g = 31 - g;
    b = 31 - b;
    r = (r * amt) >> 3;
    g = (g * amt) >> 3;
    b = (b * amt) >> 3;
    r = 31 - r;
    g = 31 - g;
    b = 31 - b;
    pal[i] = r | (g << 5) | (b << 10);
  }
  sys_copy_bgpal(0, pal, 512);
  sys_copy_spritepal(0, pal, 512);
  free(pal);
}

static void palette_fadefromwhite() {
  for (i32 i = 0; i <= 8; i++) {
    palette_white(i);
    nextframe();
  }
}

static void palette_fadetowhite() {
  for (i32 i = 8; i >= 0; i--) {
    palette_white(i);
    nextframe();
  }
}

static void palette_fadefromblack() {
  for (i32 i = 0; i <= 8; i++) {
    palette_black(i);
    nextframe();
  }
}

static void palette_fadetoblack() {
  for (i32 i = 8; i >= 0; i--) {
    palette_black(i);
    nextframe();
  }
}

static i32 pause_menu() { // -1 for nothing, 0-0xff for new game difficulty, 0x100 = save+quit
  cursor_hide();
  set_option_tiles();
  g_statsel_x = 0;
  g_statsel_y = 0;
  // -144 to -104 to -24
  static i32 pause_move_amt[] = {
    1, 1, 2, 2, 3, 3, 4, 4, 4, 100, 5,
    5, 6, 6, 7, 7, 7, 7, 6, 6, 5,
    5, 4, 4, 4, 3, 3, 2, 2, 1, 1,
    -1
  };
  i32 pause_i = 0;
  for (i32 pos = -144; pause_move_amt[pause_i] >= 0; pause_i++) {
    i32 amt = pause_move_amt[pause_i];
    if (amt == 100) {
      sys_copy_map(0x1c, sizeof(stat_tiles_top), stat_tiles_bot, sizeof(stat_tiles_bot));
      draw_books();
      continue;
    }
    pos += amt;
    for (i32 j = S_HP_START; j <= S_EXP_END; j++) {
      g_sprites[j].origin.y -= amt;
    }
    nextframe();
    sys_set_bgt0_scroll(4, pos);
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
    } else if ((g_hit & SYS_INPUT_B) || (g_hit & SYS_INPUT_ST)) {
      break;
    } else if (g_hit & SYS_INPUT_A) {
      if (g_statsel_y == 0) {
        // top menu
        switch (g_statsel_x >> 1) {
          case 0: // save+quit
            cursor_hide();
            return 0x100;
          case 1: { // new game
            cursor_hide();
            i32 diff = popup_newgame();
            if (diff >= 0) {
              return diff;
            } else {
              cursor_show();
            }
            break;
          }
          case 2: { // music
            u16 v = volume_map_back[saveroot.songvol];
            if (v == 10) v = 0; else v++;
            saveroot.songvol = volume_map_fwd[v];
            snd_set_song_volume(saveroot.songvol);
            set_option_tiles();
            break;
          }
          case 3: { // sfx
            u16 v = volume_map_back[saveroot.sfxvol];
            if (v == 10) v = 0; else v++;
            saveroot.sfxvol = volume_map_fwd[v];
            snd_set_sfx_volume(saveroot.sfxvol);
            sfx_bump();
            set_option_tiles();
            break;
          }
          case 4: // brightness
            if (saveroot.brightness == 9) saveroot.brightness = 0;
            else saveroot.brightness++;
            palette_black(8);
            set_option_tiles();
            break;
        }
      } else {
        for (i32 b = 0; b < B__SIZE; b++) {
          i32 bi = book_info(b, BI_DRAW);
          i32 x = bi & 0xf;
          i32 y = (bi >> 4) & 0xf;
          if (g_statsel_x == x && g_statsel_y - 1 == y) {
            book_click(b, 1);
            break;
          }
        }
      }
    }
  }
  cursor_hide();
  pause_i--;
  for (i32 pos = -24; pause_i >= 0; pause_i--) {
    i32 amt = pause_move_amt[pause_i];
    if (amt == 100) {
      sys_copy_map(0x1c, sizeof(stat_tiles_top), stat_tiles_bot0, sizeof(stat_tiles_bot0));
      continue;
    }
    pos -= amt;
    for (i32 j = S_HP_START; j <= S_EXP_END; j++) {
      g_sprites[j].origin.y += amt;
    }
    nextframe();
    sys_set_bgt0_scroll(4, pos);
  }
  if (!game->win) {
    cursor_to_gamesel();
    cursor_show();
  }
  return -1;
}

static i32 note_menu() {
  i32 mx = 0, my = 0;
  i32 popx = 0;
  i32 popy = game->sely * 16 - 18;
  if (popy < 1) popy = 1;
  if (popy > 91) popy = 91;
  if (game->selx < BOARD_CW) {
    popx = game->selx * 16 + 29;
  } else {
    popx = game->selx * 16 - 60;
  }
  cursor_pause();
  g_sprites[S_POPUP].pc = ani_note;
  g_sprites[S_POPUP].origin.x = popx;
  g_sprites[S_POPUP].origin.y = popy;
  g_sprites[S_POPUPCUR].pc = ani_notecur;
  for (;;) {
    g_sprites[S_POPUPCUR].origin.x = popx + 1 + mx * 15;
    g_sprites[S_POPUPCUR].origin.y = popy + 2 + my * 14;
    nextframe();
    if (g_hit & SYS_INPUT_A) {
      break;
    } else if ((g_hit & SYS_INPUT_B) || (g_hit & SYS_INPUT_ST)) {
      mx = -1;
      my = 0;
      break;
    } else if (g_hit & SYS_INPUT_U) {
      if (my > 0) my--;
    } else if (g_hit & SYS_INPUT_R) {
      if (mx < 3) mx++;
    } else if (g_hit & SYS_INPUT_D) {
      if (my < 3) my++;
    } else if (g_hit & SYS_INPUT_L) {
      if (mx > 0) mx--;
    }
  }
  g_sprites[S_POPUP].pc = NULL;
  g_sprites[S_POPUPCUR].pc = NULL;
  cursor_show();
  return my * 4 + mx;
}

static i32 title_screen() { // -1 = continue, 0-0xff = new game difficulty
  load_scr(scr_title_o);

  load_savecopy();
  u32 cs = calculate_checksum(&savecopy);
  bool valid_save = savecopy.checksum == cs;
  bool has_file = valid_save && savecopy.game.win == 0;
  if (valid_save) {
    memcpy8(&saveroot, &savecopy, sizeof(struct save_st));
    snd_set_song_volume(saveroot.songvol);
    snd_set_sfx_volume(saveroot.sfxvol);
  }

  palette_fadefromwhite();

  const i32 MENU_Y = 120;
  const i32 MENU_X = 92;

  // cursor
  g_sprites[S_TITLE_START + 0].pc = ani_arrowr;
  g_sprites[S_TITLE_START + 0].origin.x = MENU_X;

  i32 menu = 0;

  for (;;) {
    g_sprites[S_TITLE_START + 0].origin.y = (MENU_Y - 5) + menu * 9;
    u32 next_spr = 1;
    #define ADDSPR(pc1, pc2)  do {                                   \
        i32 sy = MENU_Y + ((next_spr - 1) >> 1) * 9;                 \
        g_sprites[S_TITLE_START + next_spr].pc = pc1;                \
        g_sprites[S_TITLE_START + next_spr].origin.x = MENU_X + 12;  \
        g_sprites[S_TITLE_START + next_spr].origin.y = sy;           \
        next_spr++;                                                  \
        g_sprites[S_TITLE_START + next_spr].pc = pc2;                \
        g_sprites[S_TITLE_START + next_spr].origin.x = MENU_X + 12;  \
        g_sprites[S_TITLE_START + next_spr].origin.y = sy;           \
        next_spr++;                                                  \
      } while (0)
    if (valid_save) {
      if (has_file) {
        ADDSPR(ani_title_continue1, ani_title_continue2);
      }
      ADDSPR(ani_title_newgame1, ani_title_newgame2);
      ADDSPR(ani_title_delete1, ani_title_delete2);
    } else {
      ADDSPR(ani_title_newgame1, ani_title_newgame2);
    }
    while (next_spr <= 6) {
      ADDSPR(NULL, NULL);
    }
    #undef ADDSPR
    nextframe();
    if (g_hit & SYS_INPUT_U) {
      if (menu > 0) {
        menu--;
      }
    } else if (g_hit & SYS_INPUT_D) {
      if (menu < (valid_save ? has_file ? 2 : 1 : 0)) {
        menu++;
      }
    } else if ((g_hit & SYS_INPUT_A) || (g_hit & SYS_INPUT_ST)) {
      if (
        valid_save && (
        (has_file && menu == 2) ||
        (!has_file && menu == 1)
      )) { // delete?
        g_sprites[S_TITLE_START + 0].pc = NULL;
        bool del = !!popup_delete();
        g_sprites[S_TITLE_START + 0].pc = ani_arrowr;
        if (del) {
          // delete!
          save_savecopy(true);
          saveroot.books = RESET_BOOKS;
          saveroot.game.win = 3;
          valid_save = false;
          menu = 0;
        }
      } else if (valid_save && has_file && menu == 0) { // continue
        g_sprites[S_TITLE_START + 0].pc = NULL;
        palette_fadetoblack();
        menu = -1;
        break;
      } else { // new game
        g_sprites[S_TITLE_START + 0].pc = NULL;
        book_award(B_INTRO, 0);
        i32 diff = popup_newgame();
        if (diff < 0) {
          g_sprites[S_TITLE_START + 0].pc = ani_arrowr;
        } else {
          menu = diff;
          palette_fadetoblack();
          break;
        }
      }
    }
  }

  // hide all title sprites
  for (i32 i = S_TITLE_START; i <= S_TITLE_END; i++) {
    g_sprites[i].pc = NULL;
  }
  return menu;
}

static void move_game_cursor(i32 dx, i32 dy) {
  cursor_to_gamesel_offset(dx, dy);
  nextframe();
  cursor_to_gamesel_offset(dx * 2, dy * 2);
  nextframe();
  i32 nx = game->selx + dx;
  i32 ny = game->sely + dy;
  game_hover(
    game,
    handler,
    nx < 0 ? BOARD_W - 1 : nx >= BOARD_W ? 0 : nx,
    ny < 0 ? BOARD_H - 1 : ny >= BOARD_H ? 0 : ny
  );
  cursor_to_gamesel_offset(-dx * 2, -dy * 2);
  nextframe();
  cursor_to_gamesel_offset(-dx, -dy);
  nextframe();
  cursor_to_gamesel();
}

void gvmain() {
  sys_init();
  saveroot.books = RESET_BOOKS;
  saveroot.game.win = 3;
  saveroot.brightness = 2;
  saveroot.songvol = 6;
  saveroot.sfxvol = 16;
  snd_set_master_volume(16);
  snd_load_song(BINADDR(song1_gvsong), 2); // silence
  snd_set_song_volume(saveroot.songvol);
  snd_set_sfx_volume(saveroot.sfxvol);
  sys_set_vblank(irq_vblank);
  sys_copy_tiles(4, 0, BINADDR(sprites_bin), BINSIZE(sprites_bin));
  gfx_showscreen(true);
  palette_white(0);
  i32 load;
start_title:
  load = title_screen();
start_game:
  if (load >= 0) {
    load_level(load, rnd32(&g_rnd));
  }
  g_showing_levelup = false;
  draw_level();
  palette_fadefromblack();

  i32 levelup_cooldown = 0;
  i32 next_tile_update = 5;
  i32 hint_cooldown = 0;
  i32 hint_cooldown_max = 15;
  for (;;) {
    if (levelup_cooldown > 0) levelup_cooldown--;
    if (next_tile_update > 0) {
      next_tile_update--;
      if (next_tile_update == 0) {
        i32 x = roll(&g_rnd, BOARD_W);
        i32 y = roll(&g_rnd, BOARD_H);
        tile_update(x, y);
        next_tile_update = roll(&g_rnd, 7) + 5;
      }
    }
    nextframe();
    if (game->win) {
      // dead or won
      if ((g_hit & SYS_INPUT_A) || (g_hit & SYS_INPUT_ST)) {
        i32 diff = popup_newgame();
        if (diff >= 0) {
          load = diff;
          palette_fadetoblack();
          goto start_game;
        }
      }
    } else {
      // play the game!
      if (g_cheat && (g_down & SYS_INPUT_ZL) && !(game->difficulty & D_ONLYMINES)) {
        if (hint_cooldown > 0) {
          hint_cooldown--;
        } else {
          // let the computer play
          hint_cooldown = hint_cooldown_max;
          if (hint_cooldown_max > 0) hint_cooldown_max--;
          i32 hint = game_hint(game, handler, -1);
          u8 x = hint & 0xff;
          u8 y = (hint >> 8) & 0xff;
          u8 action = (hint >> 16) & 0xff;
          i8 note = (hint >> 24) & 0xff;
          //sys_print("hint: %x %x %x %x", x, y, action, note);
          switch (action) {
            case 0: // click
              game_hover(game, handler, x, y);
              cursor_to_gamesel();
              if (!game_click(game, handler)) {
                sfx_bump();
              }
              break;
            case 1: { // note
              game_hover(game, handler, x, y);
              cursor_to_gamesel();
              game_note(game, handler, note);
              break;
            }
            case 2: // levelup
              if (!game_levelup(game, handler)) {
                sfx_bump();
              }
              break;
            default:
              sfx_bump();
              hint_cooldown = hint_cooldown_max = 30;
              break;
          }
        }
      } else {
        hint_cooldown = 0;
        hint_cooldown_max = 15;
      }
      if (g_cheat && (g_hit & SYS_INPUT_ZR)) {
        set_peek(!g_peek);
      }
      if (g_hit & SYS_INPUT_U) {
        move_game_cursor(0, -1);
      } else if (g_hit & SYS_INPUT_R) {
        move_game_cursor(1, 0);
      } else if (g_hit & SYS_INPUT_D) {
        move_game_cursor(0, 1);
      } else if (g_hit & SYS_INPUT_L) {
        move_game_cursor(-1, 0);
      } else if (g_hit & SYS_INPUT_SE) {
        if (!(game->difficulty & D_ONLYMINES) && !levelup_cooldown) {
          if (game_levelup(game, handler)) {
            levelup_cooldown = 45;
          } else {
            sfx_bump();
          }
        }
      } else if (g_hit & SYS_INPUT_B) {
        u32 k = game->selx + game->sely * BOARD_W;
        if (GET_STATUS(game->board[k]) == S_HIDDEN) {
          i32 k = game->selx + game->sely * BOARD_W;
          i32 r = game->notes[k] == -2 ? 15 : 14; // toggle mine/blank
          if (!(game->difficulty & D_ONLYMINES)) {
            r = note_menu();
          }
          if (r >= 0) {
            i8 v;
            if (r < 13) {
              v = r + 1;
            } else if (r == 13) { // question mark
              v = -1;
            } else if (r == 14) { // mine
              v = -2;
            } else { // r == 15, blank
              v = -3;
            }
            game_note(game, handler, v);
          }
        } else {
          sfx_bump();
        }
      } else if (g_hit & SYS_INPUT_A) {
        if (!game_click(game, handler)) {
          sfx_bump();
        }
      } else if (g_hit & SYS_INPUT_ST) {
        i32 p = pause_menu();
        if (p >= 0 && p < 0x100) {
          // new game
          palette_fadetoblack();
          load = p;
          goto start_game;
        } else if (p == 0x100) {
          // save+quit
          palette_fadetowhite();
          save_savecopy(false);
          for (i32 king = 0; king < 4; king++) {
            g_sprites[S_KING1_BODY + king].pc = NULL;
          }
          goto start_title;
        }
      }
    }
  }
}

static u16 restore_oam[0x200];
static u16 restore_vram[240 * 80] SECTION_EWRAM;
static i32 restore_mode;
static u16 *const VRAM = (u16 *)0x06000000;
static void book_click_start() {
  palette_fadetoblack();
  memcpy32(restore_oam, g_oam, 0x400);
  memcpy32(restore_vram, VRAM, 240 * 160);
}

static void book_click_end() {
  memcpy32(g_oam, restore_oam, 0x400);
  memcpy32(VRAM, restore_vram, 240 * 160);
  if (restore_mode == 0) { // title screen
    gfx_showobj(true);
  } else { // pause menu or win
    set_game_gfx();
    sys_set_bgt3_scroll(8, 13);
    sys_set_bgt2_scroll(8, 13);
    sys_set_bgt1_scroll(8, 10);
    sys_set_bgt0_scroll(4, restore_mode == 1 ? -24 : -144);
  }
  palette_fadefromblack();
}

static i32 book_click_single_raw(const void *addr, u32 size) {
  book_click_start();
  book_scr_raw(addr, size);
  book_click_end();
  return 0;
}
#define book_click_single(a) book_click_single_raw(BINADDR(a), BINSIZE(a))

static void book_click(enum book_enum book, i32 return_mode) {
  if (!(saveroot.books & (1 << book))) {
    // doesn't have book
    sfx_bump();
    return;
  }
  restore_mode = return_mode;
  book_info(book, BI_CLICK);
}

static void book_award(enum book_enum book, i32 return_mode) {
  if (saveroot.books & (1 << book)) { // already have the book
    return;
  }
  saveroot.books |= 1 << book;
  save_savecopy(false);
  popup_show(book, 40);
  if (book == B_100) {
    waitstart();
    popup_hide(40);
    return;
  }
  g_sprites[S_POPUPCUR].pc = ani_arrowr2;
  g_sprites[S_POPUPCUR].origin.y = 87;
  i32 menu = 0;
  for (;;) {
    g_sprites[S_POPUPCUR].origin.x = 91 + menu * 30;
    nextframe();
    if (g_hit & SYS_INPUT_L) {
      if (menu > 0) menu--;
    } else if (g_hit & SYS_INPUT_R) {
      if (menu < 1) menu++;
    } else if ((g_hit & SYS_INPUT_A) || (g_hit & SYS_INPUT_ST)) {
      g_sprites[S_POPUPCUR].pc = NULL;
      popup_hide(40);
      if (menu == 0) {
        book_click(book, return_mode);
      }
      break;
    }
  }
}

static i32 count_dead(i32 type, i32 worth) {
  i32 result = 0;
  for (i32 k = 0; k < BOARD_SIZE; k++) {
    if (
      GET_TYPE(game->board[k]) == type &&
      GET_STATUS(game->board[k]) == S_PRESSED
    ) {
      result += worth;
    }
  }
  return result;
}

static i32 book_info(enum book_enum book, enum book_info_action action) {
  #define DRAW(x, y, b)  ((x) | ((y) << 4) | ((b) << 8))
  #define CHECKG()       if (game->difficulty & D_ONLYMINES) return 0
  switch (book) {
    case B_INTRO:
      switch (action) {
        case BI_DRAW: return DRAW(0, 0, 0);
        case BI_CHECK: return 0;
        case BI_CLICK:
          book_click_start();
          book_scr(scr_how1_o);
          book_scr(scr_how2_o);
          book_scr(scr_how3_o);
          book_click_end();
          return 0;
      }
      break;
    case B_ITEMS:
      switch (action) {
        case BI_DRAW: return DRAW(1, 0, 0);
        case BI_CHECK:
          CHECKG();
          // TODO: item screen should explain how to get books too (leave dead enemies around)
          return 1;
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LOWLEVEL:
      switch (action) {
        case BI_DRAW: return DRAW(2, 0, 0);
        case BI_CHECK:
          CHECKG();
          return (
            count_dead(T_LV1A, 1) +
            count_dead(T_LV2, 1) +
            count_dead(T_LV3A, 1)
          );
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LV1B:
      switch (action) {
        case BI_DRAW: return DRAW(3, 0, 0);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV1B, 10);
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LV3B:
      switch (action) {
        case BI_DRAW: return DRAW(4, 0, 0);
        case BI_CHECK:
          CHECKG();
          return (
            count_dead(T_LV3B, 5) +
            count_dead(T_ITEM_LV3B0, 5) +
            count_dead(T_ITEM_EXP6, 5)
          );
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LV3C:
      switch (action) {
        case BI_DRAW: return DRAW(5, 0, 0);
        case BI_CHECK:
          CHECKG();
          return (
            count_dead(T_LV3C, 5) +
            count_dead(T_ITEM_LV3C0, 5) +
            count_dead(T_ITEM_EXP9, 5)
          );
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LV4A:
      switch (action) {
        case BI_DRAW: return DRAW(6, 0, 0);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV4A, 10);
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LV4B:
      switch (action) {
        case BI_DRAW: return DRAW(7, 0, 0);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV4B, 10);
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LV4C:
      switch (action) {
        case BI_DRAW: return DRAW(8, 0, 0);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV4C, 10);
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LV5A:
      switch (action) {
        case BI_DRAW: return DRAW(9, 0, 0);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV5A, 10);
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LV5B:
      switch (action) {
        case BI_DRAW: return DRAW(0, 1, 0);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV5B, 10);
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LV5C:
      switch (action) {
        case BI_DRAW: return DRAW(1, 1, 0);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV5C, 10);
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LV6:
      switch (action) {
        case BI_DRAW: return DRAW(2, 1, 0);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV6, 10);
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LV7:
      switch (action) {
        case BI_DRAW: return DRAW(3, 1, 0);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV7, 10);
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LV8:
      switch (action) {
        case BI_DRAW: return DRAW(4, 1, 0);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV8, 10);
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LV9:
      switch (action) {
        case BI_DRAW: return DRAW(5, 1, 0);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV9, 10);
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LV10:
      switch (action) {
        case BI_DRAW: return DRAW(6, 1, 0);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV10, 10);
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LV11:
      switch (action) {
        case BI_DRAW: return DRAW(7, 1, 0);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV11, 10);
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LV13:
      switch (action) {
        case BI_DRAW: return DRAW(8, 1, 0);
        case BI_CHECK:
          CHECKG();
          return 50;
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_MINE:
      switch (action) {
        case BI_DRAW: return DRAW(9, 1, 0);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LAVA, 1) ? 0 : 10; // award mines if we *didn't* blow them up
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_WALL:
      switch (action) {
        case BI_DRAW: return DRAW(0, 2, 0);
        case BI_CHECK:
          CHECKG();
          return game->difficulty >= 2 && game->difficulty <= 4 ? 10 : 0;
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_CHEST:
      switch (action) {
        case BI_DRAW: return DRAW(1, 2, 0);
        case BI_CHECK:
          CHECKG();
          return (
            count_dead(T_CHEST_HEAL, 10) +
            count_dead(T_CHEST_EYE2, 10) +
            count_dead(T_CHEST_EXP, 10)
          );
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_LAVA:
      switch (action) {
        case BI_DRAW: return DRAW(2, 2, 0);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LAVA, 10);
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_EASY:
      switch (action) {
        case BI_DRAW: return DRAW(3, 2, 0);
        case BI_CHECK: return game->difficulty == 0 ? 999 : 0;
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_MILD:
      switch (action) {
        case BI_DRAW: return DRAW(4, 2, 0);
        case BI_CHECK: return game->difficulty == 1 ? 999 : 0;
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_NORMAL:
      switch (action) {
        case BI_DRAW: return DRAW(5, 2, 0);
        case BI_CHECK: return game->difficulty == 2 ? 999 : 0;
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_HARD:
      switch (action) {
        case BI_DRAW: return DRAW(6, 2, 0);
        case BI_CHECK: return game->difficulty == 3 ? 999 : 0;
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_EXPERT:
      switch (action) {
        case BI_DRAW: return DRAW(7, 2, 0);
        case BI_CHECK: return game->difficulty == 4 ? 999 : 0;
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_ONLYMINES:
      switch (action) {
        case BI_DRAW: return DRAW(8, 2, 0);
        case BI_CHECK: return (game->difficulty & D_ONLYMINES) ? 999 : 0;
        case BI_CLICK: return book_click_single(scr_how1_o);
      }
      break;
    case B_100:
      switch (action) {
        case BI_DRAW: return DRAW(9, 2, 0);
        case BI_CHECK: return 0;
        case BI_CLICK: {
          bool was_cheating = g_cheat;
          popup_cheat();
          if (was_cheating != g_cheat) {
            // TODO: reload appropriate song
          }
          if (g_cheat) {
            // TODO: show how to cheat
            book_click_single(scr_how1_o);
          }
          return 0;
        }
      }
      break;
  }
  return 0;
  #undef DRAW
  #undef CHECKG
}

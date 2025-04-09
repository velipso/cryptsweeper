//
// cryptsweeper - fight the graveyard monsters and stop death
// by Pocket Pulp (@velipso), https://pulp.biz
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
#define S_TITLE_END    24
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

enum song_enum {
  SONG_TITLE,
  SONG_SUCCESS,
  SONG_FAILURE,
  SONG_MAIN
};

enum book_enum {
  B_INTRO,
  B_ITEMS,
  B_LOWLEVEL,
  B_LV1B,
  B_LV3A,
  B_LV3BC,
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
  u32 seed;
  u32 min;
  u32 sec;
  u32 cycles;
  u8 cheated;
  u8 songvol;
  u8 sfxvol;
  u8 brightness;
  struct game_st game;
};

static u32 g_down;
static u32 g_hit;
static i32 g_cursor_x;
static i32 g_cursor_y;
static i32 g_statsel_x;
static i32 g_statsel_y;
static i32 g_next_particle = S_PART_START;
static i32 g_lava_frame = 0;
static bool g_time = false;
static bool g_peek = false;
static bool g_cheat = false;

struct save_st saveroot;
static struct game_st *const game = &saveroot.game;
struct save_st savecopy SECTION_EWRAM;
struct rnd_st g_rnd = { 1, 1 };
static bool g_showing_levelup;
static const struct {
  const i32 t;
  const u16 *const ani_f1;
  const u16 *const ani_f2;
} g_kinginfo[4] = {
  { T_LV1B, ani_lv1b_f1, ani_lv1b_f2 },
  { T_LV5B, ani_lv5b_f1, ani_lv5b_f2 },
  { T_LV10, ani_lv10_f1, ani_lv10_f2 },
  { T_LV13, ani_lv13_f1, ani_lv13_f2 }
};
static struct { i32 x, y; } g_kingxy[4];
static const u16 *const ani_statsX[] = {
  ani_stats0, ani_stats1, ani_stats2, ani_stats3,
  ani_stats4, ani_stats5, ani_stats6, ani_stats7,
  ani_stats8, ani_stats9, ani_statsA, ani_statsB,
  ani_statsC, ani_statsD, ani_statsE, ani_statsF
};
#define TU_XYZ(x, y, z)       ((x) | ((y) << 6) | ((z) << 12))
#define TU_XY(x, y)           TU_XYZ(x, y, 0)
#define TU_WAIT_NEXT(x, y)    TU_XYZ(x, y, 0)
#define TU_WAIT_A(x, y)       TU_XYZ(x, y, 1)
#define TU_WAIT_BMINE(x, y)   TU_XYZ(x, y, 2)
#define TU_WAIT_BNUM2(x, y)   TU_XYZ(x, y, 3)
#define TU_WAIT_SE(x, y)      TU_XYZ(x, y, 4)
#define TU_WAIT_TIME(t)       TU_XYZ(t, 0, 5)
#define TU_GETX(t)            ((t) & 63)
#define TU_GETY(t)            (((t) >> 6) & 63)
#define TU_GETZ(t)            (((t) >> 12) & 15)
#define TU_TEXT_SAME          ((void *)1)
static const struct {
  const char *text;
  const i16 arrow;
  const u16 wait;
} tutorial_steps[] SECTION_ROM = {
  { "You play Crypt\nSweeper\x02 on\x02 a\ngrid with hidden\nmonsters.\n"
    "\x02    A: Next\n\x03Start: Exit", -1, TU_WAIT_NEXT(22, 10) },
  { "Begin the game\nby hitting A on\nthe lantern.", TU_XY(6, 2), TU_WAIT_A(8, 6) },
  { "\x03""Some\x03 hidden\n\x03""monsters are\n\x03""now visible.", -1, TU_WAIT_NEXT(1, 6) },
  { "You start with\n5 hearts.", TU_XY(2, 9), TU_WAIT_NEXT(5, 26) },
  { "Kill the beetle\nwho is level 5.", TU_XY(7, 3), TU_WAIT_A(36, 14) },
  { "", -2, TU_WAIT_TIME(6) },
  { "\x03""Oh no!  That\n\x03""cost all your\n\x03""hearts!", TU_XY(2, 9), TU_WAIT_NEXT(5, 24) },
  { "But\x02 now\x02 you\ncan collect the\nEXP that the\nbeetle dropped\nusing A.",
    TU_XY(7, 3), TU_WAIT_A(36, 14) },
  { "", -1, TU_WAIT_TIME(4) },
  { "Look, now you\ncan level up by\nhitting Select.", TU_XY(9, 9), TU_WAIT_SE(32, 24) },
  { "", -1, TU_WAIT_TIME(2) },
  { "Your\x03 hearts\nwere restored\nby leveling up!", TU_XY(2, 9), TU_WAIT_NEXT(5, 24) },
  { "Spend\x03 your\nhearts\x03 by\nkilling monsters\nand collecting\ntheir EXP.",
    TU_XY(5, 3), TU_WAIT_A(1, 3) },
  { TU_TEXT_SAME, -2, TU_WAIT_TIME(0) },
  { TU_TEXT_SAME, TU_XY(5, 3), TU_WAIT_A(1, 3) },
  { TU_TEXT_SAME, TU_XY(4, 2), TU_WAIT_A(1, 3) },
  { TU_TEXT_SAME, -2, TU_WAIT_TIME(0) },
  { TU_TEXT_SAME, TU_XY(4, 2), TU_WAIT_A(1, 3) },
  { "Level up again.", TU_XY(9, 9), TU_WAIT_SE(32, 28) },
  { "Your maximum\nhearts\x03 has\nincreased from\n5 to 6!", TU_XY(2, 9), TU_WAIT_NEXT(5, 22) },
  { "", -1, TU_WAIT_TIME(6) },
  { " What\x03 about\n the numbers\n on the dirt?", -1, TU_WAIT_NEXT(22, 21) },
  { "They represent\nthe\x03 total\x03 of\nall surrounding\nmonsters.", -1, TU_WAIT_NEXT(22, 21) },
  { "This dirt says\n8,\x03 and\x02 2\x02 is\nvisible, so the\nremaning grass\nmust contain 6",
    TU_XY(5, 3), TU_WAIT_NEXT(16, 22) },
  { "Since you have\n6 hearts,\x02 it's\nsafe to attack\nthe grass.",
    TU_XY(4, 3), TU_WAIT_A(16, 22) },
  { TU_TEXT_SAME, TU_XY(4, 4), TU_WAIT_A(16, 22) },
  { TU_TEXT_SAME, -2, TU_WAIT_TIME(0) },
  { TU_TEXT_SAME, TU_XY(4, 4), TU_WAIT_A(16, 22) },
  { TU_TEXT_SAME, TU_XY(5, 4), TU_WAIT_A(16, 22) },
  { TU_TEXT_SAME, -2, TU_WAIT_TIME(0) },
  { TU_TEXT_SAME, TU_XY(5, 4), TU_WAIT_A(16, 22) },
  { "Good job! You\nsafely cleared\nthe area. Now\nlevel up again.", -1, TU_WAIT_SE(32, 24) },
  { "Kill the rest of\nthe monsters.", TU_XY(6, 4), TU_WAIT_A(36, 14) },
  { TU_TEXT_SAME, -2, TU_WAIT_TIME(0) },
  { TU_TEXT_SAME, TU_XY(6, 4), TU_WAIT_A(16, 21) },
  { TU_TEXT_SAME, TU_XY(5, 1), TU_WAIT_A(16, 21) },
  { TU_TEXT_SAME, -2, TU_WAIT_TIME(0) },
  { TU_TEXT_SAME, TU_XY(5, 1), TU_WAIT_A(16, 21) },
  { TU_TEXT_SAME, TU_XY(6, 1), TU_WAIT_A(16, 21) },
  { TU_TEXT_SAME, -2, TU_WAIT_TIME(0) },
  { TU_TEXT_SAME, TU_XY(6, 1), TU_WAIT_A(16, 21) },
  { TU_TEXT_SAME, TU_XY(8, 2), TU_WAIT_A(16, 21) },
  { TU_TEXT_SAME, -2, TU_WAIT_TIME(0) },
  { TU_TEXT_SAME, TU_XY(8, 2), TU_WAIT_A(16, 21) },
  { "", -1, TU_WAIT_TIME(6) },
  { "Oops, you don't\nhave any hearts\nleft,\x04 even\nthough\x03 you\nplayed perfect.",
    TU_XY(2, 9), TU_WAIT_NEXT(5, 20) },
  { "The chest can\nhelp.  Open\x02 it\nwith A to see\nwhat's inside.",
    TU_XY(7, 2), TU_WAIT_A(36, 14) },
  { "Oh\x03 good,\x04 a\nheart! This will\nrestore\x01 you\x01 to\nfull health.",
    TU_XY(7, 2), TU_WAIT_A(36, 14) },
  { "Now\x02 you\x02 can\nkeep playing...\nbut first, let's\nthink ahead.",
    -1, TU_WAIT_NEXT(22, 10) },
  { "The red dots\nshow\x03 nearby\nmines. You can\ndeduce\x03 there\nis a mine here.",
    TU_XY(4, 1), TU_WAIT_NEXT(10, 14) },
  { "Hover over the\ntile, hit B, and\nmark\x03 it\x03 with\n*\x03 so\x03 you\nremember.",
    TU_XY(4, 1), TU_WAIT_BMINE(10, 14) },
  { "\x02""You\x01 can\x01 also\n\x02""deduce\x03 this\n\x02""tile must be\n\x02"
    "a\x03 level\x03 2\n\x02""monster.", TU_XY(7, 4), TU_WAIT_NEXT(36, 14) },
  { " Mark it with\n a @ using B.", TU_XY(7, 4), TU_WAIT_BNUM2(36, 14) },
  { "If\x03 you\x03 can\nlevel\x02 up\x02 high\nenough, you can\ndefeat Death!",
    -1, TU_WAIT_NEXT(22, 5) },
  { "But\x03 you\x03 will\nprobably\x03 die,\nuntil you figure\nout more tricks",
    -1, TU_WAIT_NEXT(22, 5) },
  { "Play more and\npay attention!\nMonsters are\ndifferent\x03 in\nsubtle ways.",
    -1, TU_WAIT_NEXT(22, 5) },
  { "Stick to Easy\nmode\x03 for\x03 a\nwhile.\n\n\x02  Good luck!", -1, TU_WAIT_NEXT(22, 5) },
  { NULL, -1, 0 }
};

// x = pixel x
// y = row
// z = character width
static const u16 tutorial_chars[] = {
  0, // space
  TU_XYZ(40, 4, 1), // !
  0, // "
  0, // #
  0, // $
  0, // %
  0, // &
  TU_XYZ(47, 4, 1), // '
  0, // (
  0, // )
  TU_XYZ(45, 1, 8), // * "mine"
  TU_XYZ(51, 4, 5), // +
  TU_XYZ(46, 4, 1), // ,
  TU_XYZ(48, 4, 3), // -
  TU_XYZ(45, 4, 1), // .
  0, // /
  TU_XYZ( 0, 4, 4), // 0
  TU_XYZ( 4, 4, 3), // 1
  TU_XYZ( 7, 4, 4), // 2
  TU_XYZ(11, 4, 4), // 3
  TU_XYZ(15, 4, 4), // 4
  TU_XYZ(19, 4, 4), // 5
  TU_XYZ(23, 4, 4), // 6
  TU_XYZ(27, 4, 4), // 7
  TU_XYZ(31, 4, 4), // 8
  TU_XYZ(35, 4, 4), // 9
  TU_XYZ(39, 4, 1), // :
  0, // ;
  0, // <
  0, // =
  0, // >
  TU_XYZ(41, 4, 4), // ?
  TU_XYZ(53, 1, 7), // @ "2"
  TU_XYZ( 0, 0, 4), // A
  TU_XYZ( 4, 0, 4), // B
  TU_XYZ( 8, 0, 4), // C
  TU_XYZ(12, 0, 4), // D
  TU_XYZ(16, 0, 4), // E
  TU_XYZ(20, 0, 4), // F
  TU_XYZ(24, 0, 4), // G
  TU_XYZ(28, 0, 4), // H
  TU_XYZ(32, 0, 3), // I
  TU_XYZ(35, 0, 4), // J
  TU_XYZ(39, 0, 4), // K
  TU_XYZ(43, 0, 4), // L
  TU_XYZ(47, 0, 5), // M
  TU_XYZ(52, 0, 4), // N
  TU_XYZ(56, 0, 4), // O
  TU_XYZ(60, 0, 4), // P
  TU_XYZ( 0, 1, 4), // Q
  TU_XYZ( 4, 1, 4), // R
  TU_XYZ( 8, 1, 4), // S
  TU_XYZ(12, 1, 5), // T
  TU_XYZ(17, 1, 4), // U
  TU_XYZ(21, 1, 5), // V
  TU_XYZ(26, 1, 5), // W
  TU_XYZ(31, 1, 5), // X
  TU_XYZ(36, 1, 5), // Y
  TU_XYZ(41, 1, 4), // Z
  0, // [
  0, // \.
  0, // ]
  0, // ^
  0, // _
  0, // `
  TU_XYZ( 0, 2, 3), // a
  TU_XYZ( 3, 2, 3), // b
  TU_XYZ( 6, 2, 3), // c
  TU_XYZ( 9, 2, 3), // d
  TU_XYZ(12, 2, 3), // e
  TU_XYZ(15, 2, 2), // f
  TU_XYZ(17, 2, 3), // g
  TU_XYZ(20, 2, 3), // h
  TU_XYZ(23, 2, 1), // i
  TU_XYZ(24, 2, 3), // j
  TU_XYZ(27, 2, 3), // k
  TU_XYZ(30, 2, 1), // l
  TU_XYZ(31, 2, 5), // m
  TU_XYZ(36, 2, 3), // n
  TU_XYZ(39, 2, 3), // o
  TU_XYZ(42, 2, 3), // p
  TU_XYZ(45, 2, 3), // q
  TU_XYZ(48, 2, 2), // r
  TU_XYZ(50, 2, 3), // s
  TU_XYZ(53, 2, 3), // t
  TU_XYZ(56, 2, 3), // u
  TU_XYZ(59, 2, 3), // v
  TU_XYZ( 0, 3, 5), // w
  TU_XYZ( 5, 3, 3), // x
  TU_XYZ( 8, 3, 3), // y
  TU_XYZ(11, 3, 4)  // z
};

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
  if (g_lava_frame >= 0) {
    g_lava_frame = (g_lava_frame + 1) & 127;
    if ((g_lava_frame & 7) == 0) {
      sys_copy_tiles(0, 512, BINADDR(lava_bin) + (g_lava_frame >> 2) * 640, 640);
      sys_copy_tiles(0, 2560, BINADDR(lava_bin) + ((g_lava_frame >> 2) + 1) * 640, 640);
    }
  }
  if (g_time && saveroot.min < 1000) {
    saveroot.cycles += 280896;
    if (saveroot.cycles >= (1 << 24)) {
      saveroot.cycles -= 1 << 24;
      saveroot.sec++;
      if (saveroot.sec >= 60) {
        saveroot.sec -= 60;
        saveroot.min++;
      }
    }
  }
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
  save_read(&savecopy, sizeof(struct save_st));
}

static inline void save_savecopy(bool del) {
  if (!del) {
    memcpy8(&savecopy, &saveroot, sizeof(struct save_st));
  }
  savecopy.checksum = calculate_checksum(&savecopy);
  if (del) {
    // if deleting, corrupt the checksum
    savecopy.game.win = 2;
    savecopy.checksum ^= 0xaa55a5a5;
  }
  save_write(&savecopy, sizeof(struct save_st));
}

static void play_song(enum song_enum song, bool restart) {
  static enum song_enum current = (enum song_enum)999;
  if (!restart && song == current) {
    return;
  }
  current = song;
  switch (song) {
    case SONG_TITLE:
      snd_load_song(BINADDR(song2_gvsong), 3);
      break;
    case SONG_SUCCESS:
      snd_load_song(BINADDR(song2_gvsong), 1);
      break;
    case SONG_FAILURE:
      snd_load_song(BINADDR(song2_gvsong), 2);
      break;
    case SONG_MAIN:
      if (g_cheat) {
        snd_load_song(BINADDR(song1_gvsong), 0);
      } else {
        snd_load_song(BINADDR(song2_gvsong), 0);
      }
      break;
  }
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
    else if ((g_hit & SYS_INPUT_ST) || (g_hit & SYS_INPUT_A)) break;
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

static u32 dig999(i32 num) {
  if (num < 0) num = 0;
  if (num > 999) num = 999;
  u32 d0 =
    num < 100 ? 0 :
    num < 200 ? 1 :
    num < 300 ? 2 :
    num < 400 ? 3 :
    num < 500 ? 4 :
    num < 600 ? 5 :
    num < 700 ? 6 :
    num < 800 ? 7 :
    num < 900 ? 8 : 9;
  return (d0 << 8) | dig99(num - d0 * 100);
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
      roll(&g_rnd, 2) ? ani_brown1 : ani_brown2
    );
  }
}

static void place_particles_lava(i32 x, i32 y) {
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
      roll(&g_rnd, 2) ? ani_red1 : ani_red2
    );
  }
}

static void place_particles_explode(i32 x, i32 y) {
  x = x * 16 + 8;
  y = y * 16 + 3;
  for (i32 i = 0; i < 30; i++) {
    i32 dx = roll(&g_rnd, 32) - 8;
    place_particle(
      x + dx,
      y + roll(&g_rnd, 16),
      (dx < 8 ? -1 : 1) * (roll(&g_rnd, 0x0080) + 0x0080),
      -0x00f0 - roll(&g_rnd, 0x00c0),
      0x000c,
      dx < 8 ? ani_explodeL : ani_explodeR
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

static inline void cursor_to_gamesel() {
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
  #define EXP_SPRITE(bg)  (                \
    exp_i < max                            \
    ? exp_i < cur                          \
    ? (bg ? ani_expfull2 : ani_expfull)    \
    : (bg ? ani_expempty2 : ani_expempty)  \
    : NULL                                 \
  )
  if (cur < 13) {
    // put empty in the background
    i32 exp_i = 12;
    for (i32 i = 0; i < 13; i++, exp_i--) {
      g_sprites[S_EXP_START + i].pc = EXP_SPRITE(false);
      g_sprites[S_EXP_START + i].origin.x = exp_x + (12 - i) * 8;
      g_sprites[S_EXP_START + i].origin.y = exp_y + 2;
    }
    exp_i = 24;
    for (i32 i = 13; i < 25; i++, exp_i--) {
      g_sprites[S_EXP_START + i].pc = EXP_SPRITE(true);
      g_sprites[S_EXP_START + i].origin.x = exp_x + 4 + (24 - i) * 8;
      g_sprites[S_EXP_START + i].origin.y = exp_y;
    }
  } else {
    i32 exp_i = 24;
    for (i32 i = 0; i < 12; i++, exp_i--) {
      g_sprites[S_EXP_START + i].pc = EXP_SPRITE(false);
      g_sprites[S_EXP_START + i].origin.x = exp_x + 4 + (11 - i) * 8;
      g_sprites[S_EXP_START + i].origin.y = exp_y + 2;
    }
    for (i32 i = 12; i < 25; i++, exp_i--) {
      g_sprites[S_EXP_START + i].pc = EXP_SPRITE(true);
      g_sprites[S_EXP_START + i].origin.x = exp_x + (24 - i) * 8;
      g_sprites[S_EXP_START + i].origin.y = exp_y;
    }
  }
  #undef EXP_SPRITE
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
  saveroot.seed = seed;
  saveroot.min = 0;
  saveroot.sec = 0;
  saveroot.cycles = 0;
  saveroot.cheated = 0;
  g_peek = false;
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

static void load_tutorial() {
  load_level(0, 10);
  // stomp the level with hard-coded tutorial, in case the level generation changes
  game->selx = 6;
  game->sely = 2;
  const u8 board[] = {
    0x00, 0x00, 0x0b, 0x19, 0x0e, 0x05, 0x00, 0x00, 0x17, 0x02, 0x17, 0x00, 0x15, 0x02,
    0x04, 0x05, 0x02, 0x0f, 0x15, 0x04, 0x02, 0x16, 0x16, 0x0f, 0x04, 0x08, 0x08, 0x02,
    0x00, 0x00, 0x17, 0x05, 0x04, 0x00, 0x9b, 0x17, 0x02, 0x08, 0x08, 0x04, 0x17, 0x04,
    0x00, 0x0e, 0x02, 0x04, 0x00, 0x05, 0x00, 0x0b, 0x05, 0x00, 0x0e, 0x00, 0x00, 0x0e,
    0x0b, 0x0b, 0x08, 0x08, 0x05, 0x05, 0x04, 0x04, 0x15, 0x00, 0x19, 0x0b, 0x00, 0x00,
    0x04, 0x15, 0x00, 0x0b, 0x15, 0x15, 0x00, 0x54, 0x15, 0x00, 0x0b, 0x16, 0x00, 0x15,
    0x02, 0x17, 0x00, 0x0f, 0x00, 0x11, 0x00, 0x02, 0x11, 0x0f, 0x15, 0x16, 0x04, 0x0b,
    0x00, 0x0e, 0x02, 0x16, 0x08, 0x0b, 0x10, 0x10, 0x10, 0x0c, 0x17, 0x00, 0x05, 0x18,
    0x0b, 0x15, 0x19, 0x16, 0x08, 0x02, 0x10, 0x03, 0x10, 0x02, 0x17, 0x00, 0x0e, 0x12,
    0, 0 // align to 32 bits
  };
  for (i32 i = 0; i < BOARD_SIZE; i++) {
    game->board[i] = board[i];
  }
}

static void load_scr_raw(const void *addr, u32 size, bool showobj) {
  g_lava_frame = -1;
  gfx_setmode(GFX_MODE_2I);
  gfx_showbg0(false);
  gfx_showbg1(false);
  sys_set_bgs2_scroll(0, 0);
  gfx_showbg2(true);
  gfx_showbg3(false);
  gfx_showobj(showobj);
  sys_copy_tiles(0, 0, addr, size);
}
#define load_scr(a)  load_scr_raw(BINADDR(a), BINSIZE(a), true)
#define load_scr2(a) load_scr_raw(BINADDR(a), BINSIZE(a), false)

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
  i32 frame = IS_MONSTER(t) && GET_TYPE(t) != T_LV11 ? (rnd32(&g_rnd) & 1) : 0;

  if (g_peek) {
    saveroot.cheated = 1;
    if (GET_TYPE(t) == T_LV11) frame = 1;
  }

  for (i32 king = 0; king < 4; king++) {
    if (x == g_kingxy[king].x && y == g_kingxy[king].y) {
      if (
        GET_TYPE(t) != g_kinginfo[king].t ||
        (!g_peek && !game->win && GET_STATUS(t) == S_HIDDEN)
      ) {
        g_sprites[S_KING1_BODY + king].pc = NULL;
      } else {
        if (GET_STATUS(t) == S_PRESSED) {
          g_sprites[S_KING1_BODY + king].pc = NULL;
        } else {
          g_sprites[S_KING1_BODY + king].pc =
            frame == 0 ? g_kinginfo[king].ani_f1 : g_kinginfo[king].ani_f2;
        }
        g_sprites[S_KING1_BODY + king].origin.x = g_kingxy[king].x * 16 + 8;
        g_sprites[S_KING1_BODY + king].origin.y = g_kingxy[king].y * 16 + 3;
      }
    }
  }

  switch (GET_STATUS(t)) {
    case S_HIDDEN:
      place_floor(x, y, 0);
      if (game->win) {
        place_answer(x, y, frame);
        place_number(x, y, 0, -3);
      } else {
        if (g_peek) {
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
  if (g_peek == f) return;
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
  #define HP_SPRITE(bg)  (               \
    hp_i >= max                          \
    ? NULL                               \
    : hp_i < cur                         \
    ? (bg ? ani_hpfull2 : ani_hpfull)    \
    : (bg ? ani_hpempty2 : ani_hpempty)  \
  )
  if (cur <= 7) {
    // put empty in the background
    i32 hp_i = 6;
    for (i32 i = 0; i < 7; i++, hp_i--) {
      g_sprites[S_HP_START + i].pc = HP_SPRITE(false);
      g_sprites[S_HP_START + i].origin.x = hp_x + (6 - i) * 8;
      g_sprites[S_HP_START + i].origin.y = hp_y + 2;
    }
    hp_i = 12;
    for (i32 i = 7; i < 13; i++, hp_i--) {
      g_sprites[S_HP_START + i].pc = HP_SPRITE(true);
      g_sprites[S_HP_START + i].origin.x = hp_x + 4 + (12 - i) * 8;
      g_sprites[S_HP_START + i].origin.y = hp_y;
    }
  } else {
    i32 hp_i = 12;
    for (i32 i = 0; i < 6; i++, hp_i--) {
      g_sprites[S_HP_START + i].pc = HP_SPRITE(false);
      g_sprites[S_HP_START + i].origin.x = hp_x + 4 + (5 - i) * 8;
      g_sprites[S_HP_START + i].origin.y = hp_y + 2;
    }
    for (i32 i = 6; i < 13; i++, hp_i--) {
      g_sprites[S_HP_START + i].pc = HP_SPRITE(true);
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
  if (!(saveroot.game.difficulty & D_ONLYMINES)) {
    sfx_youdie();
  }
  g_time = false;
  play_song(SONG_FAILURE, false);
  cursor_hide();
  if (GET_TYPE(game->board[x + y * BOARD_W]) == T_MINE) {
    shake_screen();
    place_particles_explode(x, y);
  }
  save_savecopy(false);
}

static void hide_time_seed() {
  for (i32 s = 0; s < 21; s++) {
    g_sprites[S_PART_START + s].pc = NULL;
  }
}

static void draw_time_seed(bool classic) {
  const void *popup_addr = BINADDR(popups_bin);
  popup_addr += 8 * 512 * 33 + (saveroot.cheated ? 512 * 4 : 0);
  for (i32 i = 0; i < 4; i++, popup_addr += 512) {
    sys_copy_tiles(4, 16384 + i * 1024, popup_addr, 512);
  }
  i32 s = S_PART_START;
  i32 x = 78;
  i32 y = classic ? 143 : 8;

  #define PUSH(apc, dx)  do {     \
      g_sprites[s].pc = apc;      \
      g_sprites[s].origin.x = x;  \
      g_sprites[s].origin.y = y;  \
      s++;                        \
      x += dx;                    \
    } while (0)

  // "TIME"
  PUSH(ani_statstime, 30);

  { // minutes
    i32 min = saveroot.min >= 1000 ? 999 : saveroot.min;
    i32 d999 = dig999(min);
    i32 d0 = d999 >> 8;
    i32 d1 = (d999 >> 4) & 15;
    i32 d2 = d999 & 15;
    if (d0 > 0) {
      PUSH(ani_statsX[d0], 6);
    } else {
      x += 6;
    }
    if (d0 > 0 || d1 > 0) {
      PUSH(ani_statsX[d1], 6);
    } else {
      x += 6;
    }
    PUSH(ani_statsX[d2], 7);
  }
  // ":"
  PUSH(ani_statscol, 5);
  { // seconds
    i32 sec = saveroot.min >= 1000 ? 59 : saveroot.sec;
    i32 d99 = dig99(sec);
    PUSH(ani_statsX[d99 >> 4], 6);
    PUSH(ani_statsX[d99 & 15], 7);
  }
  PUSH(ani_statsper, 5);
  { // milliseconds
    i32 ms = saveroot.min >= 1000 ? 99 : ((saveroot.cycles * 100) >> 24);
    i32 d99 = dig99(ms);
    PUSH(ani_statsX[d99 >> 4], 6);
    PUSH(ani_statsX[d99 & 15], 7);
  }

  x = 78;
  y += 8;

  // "SEED"
  PUSH(ani_statsseed, 30);
  { // difficulty
    i32 d = (saveroot.game.difficulty & D_ONLYMINES)
      ? 10 + (saveroot.game.difficulty ^ D_ONLYMINES)
      : saveroot.game.difficulty;
    PUSH(ani_statsX[d], 6);
  }
  for (u32 i = 0, seed = saveroot.seed; i < 8; i++, seed <<= 4) {
    PUSH(ani_statsX[seed >> 28], 6);
  }
  #undef PUSH
}

static void draw_level();
static void you_win() {
  g_time = false;
  play_song(SONG_SUCCESS, false);
  cursor_hide();
  palette_fadetoblack();
  save_savecopy(false);
  for (i32 king = 0; king < 4; king++) {
    g_sprites[S_KING1_BODY + king].pc = NULL;
  }
  if (game->difficulty & D_ONLYMINES) {
    load_scr2(scr_winmine_o);
    draw_time_seed(true);
  } else {
    switch (game->difficulty) {
      case 0: load_scr2(scr_deasy_o); break;
      case 1: load_scr2(scr_dmild_o); break;
      case 2: load_scr2(scr_dnormal_o); break;
      case 3: load_scr2(scr_dhard_o); break;
      case 4: load_scr2(scr_dexpert_o); break;
    }
    draw_time_seed(false);
  }
  palette_fadefromblack();
  {
    bool show_time = false;
    i32 delay = 30;
    for (;;) {
      nextframe();
      if (delay > 0) delay--;
      else if ((g_hit & SYS_INPUT_ST) || (g_hit & SYS_INPUT_A)) break;
      bool should_show_time = (g_down & SYS_INPUT_ZL) || (g_down & SYS_INPUT_ZR);
      if (should_show_time != show_time) {
        show_time = should_show_time;
        gfx_showobj(show_time);
      }
    }
  }
  palette_fadetoblack();
  hide_time_seed();
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
    case EV_SHOW_LAVA:
      sfx_mine();
      shake_screen();
      break;
    case EV_HOVER_LAVA:
      sfx_lava();
      place_particles_lava(x, y);
      shake_screen();
      break;
    case EV_YOU_LOSE:    you_lose(x, y);    break;
    case EV_YOU_WIN:     you_win();         break;
    case EV_WAIT:        for (i32 i = 0; i < x; i++) nextframe(); break;
    case EV_SFX:
      switch (x) {
        case SFX_BUMP  : sfx_bump  (); break;
        case SFX_EYE   : sfx_eye   (); break;
        case SFX_CHEST : sfx_chest (); break;
        case SFX_WALL  : sfx_wall  (); break;
        case SFX_EXP1  : sfx_exp1  (); break;
        case SFX_EXP2  : sfx_exp2  (); break;
        case SFX_GRUNT1: sfx_grunt1(); break;
        case SFX_GRUNT2: sfx_grunt2(); break;
        case SFX_GRUNT3: sfx_grunt3(); break;
        case SFX_GRUNT4: sfx_grunt4(); break;
        case SFX_GRUNT5: sfx_grunt5(); break;
        case SFX_GRUNT6: sfx_grunt6(); break;
        case SFX_GRUNT7: sfx_grunt7(); break;
        case SFX_EXIT  : sfx_exit  (); break;
        case SFX_HEART : sfx_heart (); break;
        case SFX_MINE  : sfx_mine  (); break;
        case SFX_DIRT  : sfx_dirt  (); break;
        case SFX_REJECT: sfx_reject(); break;
      }
      if (x >= SFX_GRUNT1 && x <= SFX_GRUNT7) {
        g_sprites[S_POPUPCUR].pc = ani_slash;
        g_sprites[S_POPUPCUR].origin.x = saveroot.game.selx * 16 + 8;
        g_sprites[S_POPUPCUR].origin.y = saveroot.game.sely * 16 + 3;
      }
      break;
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
  g_lava_frame = 0;

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
      u32 yb = y == 0 ? 87 : 119;
      if (color < 3) {
        color *= 3;
        sys_set_map(0x1c, offset - 64, yb + color);
        sys_set_map(0x1c, offset - 62, yb + 1 + color);
        sys_set_map(0x1c, offset - 60, yb + 2 + color);
        sys_set_map(0x1c, offset + 4, 153 + color);
        sys_set_map(0x1c, offset + 68, 153 + 32 + color);
      }
      sys_set_map(0x1c, offset +  0, t);
      sys_set_map(0x1c, offset +  2, t + 1);
      sys_set_map(0x1c, offset + 64, t + 32);
      sys_set_map(0x1c, offset + 66, t + 33);
    }
  }
}

static void popup_show(i32 popup_index, i32 height) {
  sfx_swipeup();
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
  sfx_swipedown();
  height &= ~7;
  for (i32 i = height; i >= -64; i -= 8) {
    g_sprites[S_POPUP].origin.y = i;
    nextframe();
  }
  g_sprites[S_POPUP].pc = NULL;
}

static i32 popup_delete() {
  popup_show(30, 60);
  g_sprites[S_POPUPCUR].pc = ani_arrowr2;
  g_sprites[S_POPUPCUR].origin.x = 97;
  i32 menu = 0;
  for (;;) {
    g_sprites[S_POPUPCUR].origin.y = 74 + menu * 9;
    nextframe();
    if (g_hit & SYS_INPUT_U) {
      if (menu > 0) {
        menu--;
        sfx_blip();
      }
    } else if (g_hit & SYS_INPUT_D) {
      if (menu < 1) {
        menu++;
        sfx_blip();
      }
    } else if ((g_hit & SYS_INPUT_A) || (g_hit & SYS_INPUT_ST)) {
      break;
    } else if (g_hit & SYS_INPUT_B) {
      menu = 0;
      break;
    }
  }
  if (menu == 1) {
    sfx_accept();
  } else {
    sfx_reject();
  }
  g_sprites[S_POPUPCUR].pc = NULL;
  popup_hide(60);
  return menu;
}

static void popup_cheat() {
  popup_show(32, 60);
  g_sprites[S_POPUPCUR].pc = ani_arrowr2;
  g_sprites[S_POPUPCUR].origin.x = 97;
  i32 menu = g_cheat ? 1 : 0;
  for (;;) {
    g_sprites[S_POPUPCUR].origin.y = 67 + menu * 9;
    nextframe();
    if (g_hit & SYS_INPUT_U) {
      if (menu > 0) {
        menu--;
        sfx_blip();
      }
    } else if (g_hit & SYS_INPUT_D) {
      if (menu < 1) {
        menu++;
        sfx_blip();
      }
    } else if ((g_hit & SYS_INPUT_A) || (g_hit & SYS_INPUT_ST)) {
      break;
    } else if (g_hit & SYS_INPUT_B) {
      menu = 0;
      break;
    }
  }
  g_sprites[S_POPUPCUR].pc = NULL;
  if (menu) {
    sfx_accept();
  } else {
    sfx_reject();
  }
  popup_hide(60);
  bool was_cheating = g_cheat;
  g_cheat = menu == 1;
  if (g_cheat != was_cheating) {
    play_song(SONG_MAIN, true);
  }
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
        sfx_blip();
      }
    } else if (g_hit & SYS_INPUT_D) {
      if (difficulty < 4) {
        difficulty++;
        sfx_blip();
      }
    } else if ((g_hit & SYS_INPUT_A) || (g_hit & SYS_INPUT_ST)) {
      if ((g_down & SYS_INPUT_ZL) && (g_down & SYS_INPUT_ZR)) {
        difficulty |= D_ONLYMINES;
      }
      sfx_accept();
      break;
    } else if (g_hit & SYS_INPUT_B) {
      difficulty = -1;
      sfx_reject();
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

static u16 palscratch[256];
static void palette_black(i32 amt) {
  const u16 *inp = BINADDR(palette_brightness_bin) + saveroot.brightness * 512;
  for (i32 i = 0; i < 256; i++) {
    u16 c = inp[i];
    i32 r = c & 0x1f;
    i32 g = (c >> 5) & 0x1f;
    i32 b = (c >> 10) & 0x1f;
    r = (r * amt) >> 3;
    g = (g * amt) >> 3;
    b = (b * amt) >> 3;
    palscratch[i] = r | (g << 5) | (b << 10);
  }
  nextframe();
  sys_copy_bgpal(0, palscratch, 512);
  sys_copy_spritepal(0, palscratch, 512);
}

static void palette_white(i32 amt) {
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
    palscratch[i] = r | (g << 5) | (b << 10);
  }
  nextframe();
  sys_copy_bgpal(0, palscratch, 512);
  sys_copy_spritepal(0, palscratch, 512);
}

static void palette_fadefromwhite() {
  for (i32 i = 0; i <= 8; i++) {
    palette_white(i);
  }
}

static void palette_fadetowhite() {
  for (i32 i = 8; i >= 0; i--) {
    palette_white(i);
  }
}

static void palette_fadefromblack() {
  for (i32 i = 0; i <= 8; i++) {
    palette_black(i);
  }
}

static void palette_fadetoblack() {
  for (i32 i = 8; i >= 0; i--) {
    palette_black(i);
  }
}

static i32 pause_menu() { // -1 for nothing, 0-0xff for new game difficulty, 0x100 = save+quit
  sfx_slideup();
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
        sfx_blip();
      } else {
        sfx_bump();
      }
    } else if (g_hit & SYS_INPUT_R) {
      if (g_statsel_y == 0 && g_statsel_x < 8) {
        g_statsel_x += 2;
        cursor_to_statsel();
        sfx_blip();
      } else if (g_statsel_y > 0 && g_statsel_x < 9) {
        g_statsel_x++;
        cursor_to_statsel();
        sfx_blip();
      } else {
        sfx_bump();
      }
    } else if (g_hit & SYS_INPUT_D) {
      if (g_statsel_y == 0) {
        g_statsel_y = 1;
        cursor_to_statsel();
        sfx_blip();
      } else if (g_statsel_y < 3) {
        g_statsel_y++;
        cursor_to_statsel();
        sfx_blip();
      } else {
        sfx_bump();
      }
    } else if (g_hit & SYS_INPUT_L) {
      if (g_statsel_y == 0 && g_statsel_x > 1) {
        g_statsel_x -= 2;
        cursor_to_statsel();
        sfx_blip();
      } else if (g_statsel_y > 0 && g_statsel_x > 0) {
        g_statsel_x--;
        cursor_to_statsel();
        sfx_blip();
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
            sfx_accept();
            return 0x100;
          case 1: { // new game
            cursor_hide();
            sfx_accept();
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
            sfx_accept();
            set_option_tiles();
            break;
          }
          case 4: // brightness
            if (saveroot.brightness == 9) saveroot.brightness = 0;
            else saveroot.brightness++;
            palette_black(8);
            sfx_accept();
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
  sfx_slidedown();
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
  sfx_accept();
  for (;;) {
    g_sprites[S_POPUPCUR].origin.x = popx + 1 + mx * 15;
    g_sprites[S_POPUPCUR].origin.y = popy + 2 + my * 14;
    nextframe();
    if (g_hit & SYS_INPUT_A) {
      break;
    } else if ((g_hit & SYS_INPUT_B) || (g_hit & SYS_INPUT_ST)) {
      mx = -1;
      my = 0;
      sfx_reject();
      break;
    } else if (g_hit & SYS_INPUT_U) {
      if (my > 0) {
        my--;
        sfx_blip();
      }
    } else if (g_hit & SYS_INPUT_R) {
      if (mx < 3) {
        mx++;
        sfx_blip();
      }
    } else if (g_hit & SYS_INPUT_D) {
      if (my < 3) {
        my++;
        sfx_blip();
      }
    } else if (g_hit & SYS_INPUT_L) {
      if (mx > 0) {
        mx--;
        sfx_blip();
      }
    }
  }
  g_sprites[S_POPUP].pc = NULL;
  g_sprites[S_POPUPCUR].pc = NULL;
  cursor_show();
  return my * 4 + mx;
}

static i32 title_screen() { // -1 = continue, 0-0xff = new game difficulty, 0x100 = tutorial
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

  const i32 MENU_Y = 113;
  const i32 MENU_X = 92;

  // cursor
  g_sprites[S_TITLE_START + 0].pc = ani_arrowr;
  g_sprites[S_TITLE_START + 0].origin.x = MENU_X;

  i32 menu = 0;

  for (;;) {
    g_sprites[S_TITLE_START + 0].origin.y = (MENU_Y - 4) + menu * 9;
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
        ADDSPR(ani_title_continue, NULL);
      }
      ADDSPR(ani_title_newgame1, ani_title_newgame2);
      ADDSPR(ani_title_tutorial, NULL);
      ADDSPR(ani_title_credits, NULL);
      ADDSPR(ani_title_delete, NULL);
    } else {
      ADDSPR(ani_title_newgame1, ani_title_newgame2);
      ADDSPR(ani_title_tutorial, NULL);
      ADDSPR(ani_title_credits, NULL);
    }
    while (next_spr <= 10) {
      ADDSPR(NULL, NULL);
    }
    #undef ADDSPR
    nextframe();
    if (g_hit & SYS_INPUT_U) {
      if (menu > 0) {
        menu--;
        sfx_blip();
      }
    } else if (g_hit & SYS_INPUT_D) {
      if (menu < (valid_save ? has_file ? 4 : 3 : 2)) {
        menu++;
        sfx_blip();
      }
    } else if ((g_hit & SYS_INPUT_A) || (g_hit & SYS_INPUT_ST)) {
      if (
        valid_save && (
        (has_file && menu == 4) ||
        (!has_file && menu == 3)
      )) { // delete?
        g_sprites[S_TITLE_START + 0].pc = NULL;
        sfx_accept();
        bool del = !!popup_delete();
        g_sprites[S_TITLE_START + 0].pc = ani_arrowr;
        if (del) {
          // delete!
          save_savecopy(true);
          saveroot.books = RESET_BOOKS;
          saveroot.game.win = 3;
          valid_save = false;
          g_cheat = false;
          menu = 0;
        }
      } else if (valid_save && has_file && menu == 0) { // continue
        g_sprites[S_TITLE_START + 0].pc = NULL;
        sfx_accept();
        palette_fadetoblack();
        menu = -1;
        break;
      } else if (
        (valid_save && menu == (has_file ? 1 : 0)) ||
        (!valid_save && menu == 0)
      ) { // new game
        g_sprites[S_TITLE_START + 0].pc = NULL;
        sfx_accept();
        book_award(B_INTRO, 0);
        i32 diff = popup_newgame();
        if (diff < 0) {
          g_sprites[S_TITLE_START + 0].pc = ani_arrowr;
        } else {
          menu = diff;
          palette_fadetoblack();
          break;
        }
      } else if (
        (valid_save && menu == (has_file ? 2 : 1)) ||
        (!valid_save && menu == 1)
      ) { // tutorial
        sfx_accept();
        menu = 0x100;
        palette_fadetoblack();
        break;
      } else { // credits
        sfx_accept();
        palette_fadetoblack();
        load_scr2(scr_how3_o);
        palette_fadefromblack();
        waitstart();
        palette_fadetoblack();
        load_scr(scr_title_o);
        palette_fadefromblack();
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
  sfx_blip();
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

static inline u8 spix(i32 x, i32 y) {
  // read a pixel from sprites_bin
  const u8 *s = BINADDR(sprites_bin);
  return s[((x >> 3) + (y >> 3) * 16) * 64 + (x & 7) + (y & 7) * 8];
}

void gvmain() {
  sys_init();
  saveroot.books = RESET_BOOKS;
  saveroot.game.win = 3;
  saveroot.brightness = 2;
  saveroot.songvol = 6;
  saveroot.sfxvol = 16;
  snd_set_master_volume(16);
  play_song(SONG_TITLE, true);
  snd_set_song_volume(saveroot.songvol);
  snd_set_sfx_volume(saveroot.sfxvol);
  sys_set_vblank(irq_vblank);
  sys_copy_tiles(4, 0, BINADDR(sprites_bin), BINSIZE(sprites_bin));
  gfx_showscreen(true);
  palette_white(0);
  bool tutorial;
  bool tutnext;
  i32 tutstep;
  i32 load;
start_title:
  g_time = false;
  play_song(SONG_TITLE, false);
  load = title_screen();
start_game:
  tutorial = load == 0x100;
  tutstep = -1;
  tutnext = tutorial;
  g_time = false;
  if (tutorial) g_cheat = false;
  if (load >= 0) {
    if (tutorial) {
      load_tutorial();
    } else {
      load_level(load, rnd32(&g_rnd));
    }
  }
  g_showing_levelup = false;
  draw_level();
  if (tutorial) cursor_hide();
  play_song(SONG_MAIN, false);
  palette_fadefromblack();

#if 0
  if (g_down & SYS_INPUT_ZL) {
    // preview book popups
    g_cheat = true;
    for (int i = 0; i < 30; i++) {
      popup_show(i, 40);
      for (;;) {
        nextframe();
        if (g_hit & SYS_INPUT_ZL) { i = 30; break; }
        else if ((g_down & SYS_INPUT_ST) || (g_down & SYS_INPUT_A)) break;
      }
      popup_hide(40);
    }
  }
#endif

  i32 levelup_cooldown = 0;
  i32 next_tile_update = 5;
  i32 hint_cooldown = 0;
  i32 hint_cooldown_max = 15;
  i32 tut_cooldown = 0;
  g_time = true;
  for (;;) {
    if (levelup_cooldown > 0) levelup_cooldown--;
    if (tut_cooldown > 0) {
      tut_cooldown--;
      if (!tut_cooldown && TU_GETZ(tutorial_steps[tutstep].wait) == 5) {
        // advance after waiting
        tutnext = true;
      }
    }
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

    if (tutnext) {
      tutnext = false;
      tutstep++;
      tut_cooldown = 30;
      if (TU_GETZ(tutorial_steps[tutstep].wait) == 5) { // TU_WAIT_TIME
        tut_cooldown = 30 + 5 * TU_GETX(tutorial_steps[tutstep].wait);
      }
      // draw next tutorial screen
      const char *text = tutorial_steps[tutstep].text;
      if (text == NULL) {
        // tutorial is over
        tutorial = false;
        game->win = 2;
        palette_fadetowhite();
        g_sprites[S_POPUP].pc = NULL;
        g_sprites[S_POPUPCUR].pc = NULL;
        for (i32 king = 0; king < 4; king++) {
          g_sprites[S_KING1_BODY + king].pc = NULL;
        }
        goto start_title;
      } else if (text != TU_TEXT_SAME && text[0] != 0) {
        // draw the popup
        i32 lines = 1;
        for (i32 i = 0; text[i]; i++) {
          if (text[i] == '\n') lines++;
        }
        i32 pad = 4;
        i32 height = pad * 2 + lines * 9;
        static u8 popup[64 * 64];

        // draw background
        for (i32 y = 0; y < 64; y++) {
          for (i32 x = 0; x < 64; x++) {
            u8 c = 0;
            if (y < height) {
              if (y < 8) {
                c = spix(x, 128 + y);
              } else if (y >= height - 8) {
                c = spix(x, 144 + y - (height - 8));
              } else {
                c = spix(x, 137);
              }
            }
            popup[x + y * 64] = c;
          }
        }

        // draw chars
        i32 cx = 4;
        i32 cy = pad;
        for (i32 i = 0; text[i]; i++) {
          if (text[i] <= 32) {
            if (text[i] == '\n') {
              cx = 4;
              cy += 9;
            } else if (text[i] <= 9) {
              cx += text[i];
            } else if (text[i] == ' ') {
              cx += 4;
            }
          } else {
            u16 ch = tutorial_chars[text[i] - 32];
            u16 sx = TU_GETX(ch);
            u16 sy = TU_GETY(ch);
            u16 sw = TU_GETZ(ch);
            i32 offset =
              text[i] == 'g' ||
              text[i] == 'j' ||
              text[i] == 'p' ||
              text[i] == 'q' ||
              text[i] == 'y'
              ? 1 : 0;
            cy += offset;
            for (i32 py = 0; py < 8; py++) {
              for (i32 px = 0; px < sw; px++) {
                popup[cx + px + (cy + py) * 64] =
                  spix(sx + px, 152 + 8 * sy + py);
              }
            }
            cy -= offset;
            cx += sw + 1;
          }
        }

        // copy popup to VRAM
        for (i32 ty = 0; ty < 8; ty++) {
          for (i32 tx = 0; tx < 8; tx++) {
            for (i32 y = 0; y < 8; y++) {
              u32 *here = ((void *)popup) + tx * 8 + (ty * 8 + y) * 64;
              sys_copy64_tiles(4, 16384 + (tx + ty * 16) * 64 + y * 8, here[0], here[1]);
            }
          }
        }

        g_sprites[S_POPUP].pc = ani_popup;
        g_sprites[S_POPUP].origin.x = TU_GETX(tutorial_steps[tutstep].wait) * 4;
        g_sprites[S_POPUP].origin.y = TU_GETY(tutorial_steps[tutstep].wait) * 4;
      }
      if (
        !text ||
        TU_GETZ(tutorial_steps[tutstep].wait) == 0 || // TU_WAIT_NEXT
        TU_GETZ(tutorial_steps[tutstep].wait) == 5    // TU_WAIT_TIME
      ) {
        cursor_hide();
      } else {
        cursor_show();
      }
      i16 arrow = tutorial_steps[tutstep].arrow;
      if (arrow == -1) {
        g_sprites[S_POPUPCUR].pc = NULL;
      } else if (arrow >= 0) {
        g_sprites[S_POPUPCUR].pc = ani_tutarrow;
        g_sprites[S_POPUPCUR].origin.x = TU_GETX(arrow) * 16 + 8;
        g_sprites[S_POPUPCUR].origin.y = TU_GETY(arrow) * 16 + 3;
      }
    }

    bool stopmove = tutorial && (
      TU_GETZ(tutorial_steps[tutstep].wait) == 0 ||
      TU_GETZ(tutorial_steps[tutstep].wait) == 5
    );

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
          saveroot.cheated = 1;
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
              } else {
                sfx_levelup();
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
      if (g_cheat) {
        set_peek(!!(g_down & SYS_INPUT_ZR));
      }
      if (g_hit & SYS_INPUT_U) {
        if (!stopmove) move_game_cursor(0, -1);
      } else if (g_hit & SYS_INPUT_R) {
        if (!stopmove) move_game_cursor(1, 0);
      } else if (g_hit & SYS_INPUT_D) {
        if (!stopmove) move_game_cursor(0, 1);
      } else if (g_hit & SYS_INPUT_L) {
        if (!stopmove) move_game_cursor(-1, 0);
      } else if (g_hit & SYS_INPUT_SE) {
        if (tutorial) {
          if (!tut_cooldown) {
            if (TU_GETZ(tutorial_steps[tutstep].wait) == 4) { // TU_WAIT_SE
              if (game_levelup(game, handler)) {
                levelup_cooldown = 45;
                sfx_levelup();
                tutnext = true;
              } else {
                sfx_bump();
              }
            } else {
              sfx_bump();
            }
          }
        } else if (!(game->difficulty & D_ONLYMINES) && !levelup_cooldown) {
          if (game_levelup(game, handler)) {
            levelup_cooldown = 45;
            sfx_levelup();
          } else {
            sfx_bump();
          }
        }
      } else if (g_hit & SYS_INPUT_B) {
        if (tutorial && !((
          TU_GETZ(tutorial_steps[tutstep].wait) == 2 || // TU_WAIT_BMINE
          TU_GETZ(tutorial_steps[tutstep].wait) == 3    // TU_WAIT_BNUM2
        ) && (
          TU_GETX(tutorial_steps[tutstep].arrow) == game->selx &&
          TU_GETY(tutorial_steps[tutstep].arrow) == game->sely
        ))) {
          sfx_bump();
        } else {
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
              if (tutorial) {
                if (
                  (TU_GETZ(tutorial_steps[tutstep].wait) == 2 && v == -2) ||
                  (TU_GETZ(tutorial_steps[tutstep].wait) == 3 && v == 2)
                ) {
                  sfx_accept();
                  game_note(game, handler, v);
                  tutnext = true;
                } else {
                  sfx_bump();
                  // restore tutorial popup
                  tutstep--;
                  tutnext = true;
                }
              } else {
                sfx_accept();
                game_note(game, handler, v);
              }
            } else if (tutorial) {
              // restore tutorial popup
              tutstep--;
              tutnext = true;
            }
          } else {
            sfx_bump();
          }
        }
      } else if (g_hit & SYS_INPUT_A) {
        if (tutorial) {
          if (!tut_cooldown) {
            u16 wait = tutorial_steps[tutstep].wait;
            if (TU_GETZ(wait) == 0) { // TU_WAIT_NEXT
              tutnext = true;
              sfx_accept();
            } else if (
              TU_GETZ(wait) == 1 && // TU_WAIT_A
              TU_GETX(tutorial_steps[tutstep].arrow) == game->selx &&
              TU_GETY(tutorial_steps[tutstep].arrow) == game->sely
            ) {
              game_click(game, handler);
              tutnext = true;
            } else {
              sfx_bump();
            }
          }
        } else if (!game_click(game, handler)) {
          sfx_bump();
        }
      } else if (g_hit & SYS_INPUT_ST) {
        g_time = false;
        set_peek(false);
        if (tutorial) {
          if (TU_GETZ(tutorial_steps[tutstep].wait) == 4) { // TU_WAIT_SE
            sfx_bump();
          } else {
            // quit tutorial
            tutorial = false;
            game->win = 2;
            sfx_reject();
            palette_fadetowhite();
            g_sprites[S_POPUP].pc = NULL;
            g_sprites[S_POPUPCUR].pc = NULL;
            for (i32 king = 0; king < 4; king++) {
              g_sprites[S_KING1_BODY + king].pc = NULL;
            }
            goto start_title;
          }
        } else {
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
          g_time = true;
        }
      }
    }
    if (tutnext && tutorial_steps[tutstep + 1].text != TU_TEXT_SAME) {
      g_sprites[S_POPUP].pc = NULL;
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
    g_lava_frame = 0;
    set_game_gfx();
    sys_set_bgt3_scroll(8, 13);
    sys_set_bgt2_scroll(8, 13);
    sys_set_bgt1_scroll(8, 10);
    sys_set_bgt0_scroll(4, restore_mode == 1 ? -24 : -144);
  }
  palette_fadefromblack();
}

static i32 book_click_img1_raw(const void *addr, u32 size) {
  book_click_start();
  book_scr_raw(addr, size);
  book_click_end();
  return 0;
}
#define book_click_img1(a) book_click_img1_raw(BINADDR(a), BINSIZE(a))

static i32 book_click_img2_raw(const void *addr1, u32 size1, const void *addr2, u32 size2) {
  book_click_start();
  book_scr_raw(addr1, size1);
  book_scr_raw(addr2, size2);
  book_click_end();
  return 0;
}
#define book_click_img2(a, b) book_click_img2_raw(BINADDR(a), BINSIZE(a), BINADDR(b), BINSIZE(b))

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
      if (menu > 0) {
        menu--;
        sfx_blip();
      }
    } else if (g_hit & SYS_INPUT_R) {
      if (menu < 1) {
        menu++;
        sfx_blip();
      }
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
          sfx_accept();
          book_click_start();
          book_scr(scr_how1_o);
          sfx_book();
          book_scr(scr_how2_o);
          book_click_end();
          return 0;
      }
      break;
    case B_ITEMS:
      switch (action) {
        case BI_DRAW: return DRAW(1, 0, 1);
        case BI_CHECK:
          CHECKG();
          return 1;
        case BI_CLICK: sfx_book(); return book_click_img2(scr_book_items1_o, scr_book_items2_o);
      }
      break;
    case B_LOWLEVEL:
      switch (action) {
        case BI_DRAW: return DRAW(2, 0, 2);
        case BI_CHECK:
          CHECKG();
          return (
            count_dead(T_LV1A, 1) +
            count_dead(T_LV2, 1)
          );
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lowlevel_o);
      }
      break;
    case B_LV1B:
      switch (action) {
        case BI_DRAW: return DRAW(3, 0, 2);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV1B, 10);
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lv1b_o);
      }
      break;
    case B_LV3A:
      switch (action) {
        case BI_DRAW: return DRAW(4, 0, 2);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV3A, 1);
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lv3a_o);
      }
      break;
    case B_LV3BC:
      switch (action) {
        case BI_DRAW: return DRAW(5, 0, 2);
        case BI_CHECK:
          CHECKG();
          return (
            count_dead(T_LV3B, 5) +
            count_dead(T_ITEM_LV3B0, 5) +
            count_dead(T_ITEM_EXP6, 5) +
            count_dead(T_LV3C, 5) +
            count_dead(T_ITEM_LV3C0, 5) +
            count_dead(T_ITEM_EXP9, 5)
          );
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lv3bc_o);
      }
      break;
    case B_LV4A:
      switch (action) {
        case BI_DRAW: return DRAW(6, 0, 2);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV4A, 10);
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lv4a_o);
      }
      break;
    case B_LV4B:
      switch (action) {
        case BI_DRAW: return DRAW(7, 0, 2);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV4B, 10);
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lv4b_o);
      }
      break;
    case B_LV4C:
      switch (action) {
        case BI_DRAW: return DRAW(8, 0, 2);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV4C, 10);
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lv4c_o);
      }
      break;
    case B_LV5A:
      switch (action) {
        case BI_DRAW: return DRAW(9, 0, 2);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV5A, 10);
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lv5a_o);
      }
      break;
    case B_LV5B:
      switch (action) {
        case BI_DRAW: return DRAW(0, 1, 2);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV5B, 10);
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lv5b_o);
      }
      break;
    case B_LV5C:
      switch (action) {
        case BI_DRAW: return DRAW(1, 1, 2);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV5C, 10);
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lv5c_o);
      }
      break;
    case B_LV6:
      switch (action) {
        case BI_DRAW: return DRAW(2, 1, 2);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV6, 10);
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lv6_o);
      }
      break;
    case B_LV7:
      switch (action) {
        case BI_DRAW: return DRAW(3, 1, 2);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV7, 10);
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lv7_o);
      }
      break;
    case B_LV8:
      switch (action) {
        case BI_DRAW: return DRAW(4, 1, 2);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV8, 10);
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lv8_o);
      }
      break;
    case B_LV9:
      switch (action) {
        case BI_DRAW: return DRAW(5, 1, 2);
        case BI_CHECK:
          CHECKG();
          // you can't actually leave a lv9 dead because they drop required hearts
          return game->difficulty == 4 ? 10 : 0;
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lv9_o);
      }
      break;
    case B_LV10:
      switch (action) {
        case BI_DRAW: return DRAW(6, 1, 2);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV10, 10);
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lv10_o);
      }
      break;
    case B_LV11:
      switch (action) {
        case BI_DRAW: return DRAW(7, 1, 2);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LV11, 10);
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lv11_o);
      }
      break;
    case B_LV13:
      switch (action) {
        case BI_DRAW: return DRAW(8, 1, 2);
        case BI_CHECK:
          CHECKG();
          return 50;
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lv13_o);
      }
      break;
    case B_MINE:
      switch (action) {
        case BI_DRAW: return DRAW(9, 1, 1);
        case BI_CHECK:
          CHECKG();
          // award mines if we *didn't* blow them up
          return count_dead(T_LAVA, 1) ? 0 : 10;
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_mine_o);
      }
      break;
    case B_WALL:
      switch (action) {
        case BI_DRAW: return DRAW(0, 2, 1);
        case BI_CHECK:
          CHECKG();
          return game->difficulty >= 2 && game->difficulty <= 4 ? 10 : 0;
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_wall_o);
      }
      break;
    case B_CHEST:
      switch (action) {
        case BI_DRAW: return DRAW(1, 2, 1);
        case BI_CHECK:
          CHECKG();
          return game->difficulty >= 3 && game->difficulty <= 4 ? 10 : 0;
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_chest_o);
      }
      break;
    case B_LAVA:
      switch (action) {
        case BI_DRAW: return DRAW(2, 2, 1);
        case BI_CHECK:
          CHECKG();
          return count_dead(T_LAVA, 10);
        case BI_CLICK: sfx_book(); return book_click_img1(scr_book_lava_o);
      }
      break;
    case B_EASY:
      switch (action) {
        case BI_DRAW: return DRAW(3, 2, 3);
        case BI_CHECK: return game->difficulty == 0 ? 999 : 0;
        case BI_CLICK: sfx_accept(); return book_click_img1(scr_deasy_o);
      }
      break;
    case B_MILD:
      switch (action) {
        case BI_DRAW: return DRAW(4, 2, 3);
        case BI_CHECK: return game->difficulty == 1 ? 999 : 0;
        case BI_CLICK: sfx_accept(); return book_click_img1(scr_dmild_o);
      }
      break;
    case B_NORMAL:
      switch (action) {
        case BI_DRAW: return DRAW(5, 2, 3);
        case BI_CHECK: return game->difficulty == 2 ? 999 : 0;
        case BI_CLICK: sfx_accept(); return book_click_img1(scr_dnormal_o);
      }
      break;
    case B_HARD:
      switch (action) {
        case BI_DRAW: return DRAW(6, 2, 3);
        case BI_CHECK: return game->difficulty == 3 ? 999 : 0;
        case BI_CLICK: sfx_accept(); return book_click_img1(scr_dhard_o);
      }
      break;
    case B_EXPERT:
      switch (action) {
        case BI_DRAW: return DRAW(7, 2, 3);
        case BI_CHECK: return game->difficulty == 4 ? 999 : 0;
        case BI_CLICK: sfx_accept(); return book_click_img1(scr_dexpert_o);
      }
      break;
    case B_ONLYMINES:
      switch (action) {
        case BI_DRAW: return DRAW(8, 2, 3);
        case BI_CHECK: return (game->difficulty & D_ONLYMINES) ? 999 : 0;
        case BI_CLICK: sfx_accept(); return book_click_img1(scr_winmine_o);
      }
      break;
    case B_100:
      switch (action) {
        case BI_DRAW: return DRAW(9, 2, 3);
        case BI_CHECK: return 0;
        case BI_CLICK: {
          popup_cheat();
          if (g_cheat) {
            g_sprites[S_PART_START].pc = ani_ufo;
            g_sprites[S_PART_START].origin.x = 0;
            g_sprites[S_PART_START].origin.y = 0;
            book_click_start();
            load_scr_raw(BINADDR(scr_cheat_o), BINSIZE(scr_cheat_o), true);
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
            palette_fadefromblack();
            waitstart();
            palette_fadetoblack();
            g_sprites[S_PART_START].pc = NULL;
            book_click_end();
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

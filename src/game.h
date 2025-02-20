//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

//
// This library is stand-alone so it can be called from either the GBA or xform at compile-time
//

#pragma once
#include <stdint.h>
#include "rnd.h"

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;

#define BOARD_W     14
#define BOARD_CW    (BOARD_W >> 1)
#define BOARD_H     9
#define BOARD_CH    (BOARD_H >> 1)
#define BOARD_SIZE  (BOARD_W * BOARD_H)

struct game_st {
  struct rnd_st rnd;
  i8 selx;
  i8 sely;
  u8 difficulty;
  u8 level;
  u8 hp;
  u8 exp;
  u8 win; // 0 = play, 1 = dead, 2 = win
  i8 notes[BOARD_SIZE];
  u8 board[BOARD_SIZE];
  // SSTT:TTTT
  // where SS is status:
  //   00 - unpressed, hidden
  //   01 - unpressed, visible
  //   10 - pressed (if not empty, then collectable)
  //   11 - killed (the tile has killed the player)
  // and TT is type (0-63)
};

struct levelgen_st {
  struct rnd_st rnd;
  u32 unfixed_size;
  u8 board_order[BOARD_SIZE];
  u8 board[BOARD_SIZE];
  u8 unfixed[BOARD_SIZE];
};

// difficulty flags
#define D_DIFFICULTY   0x0f
#define D_ONLYMINES    0x10

// tile types
#define T_EMPTY       0
#define T_LV1A        1
#define T_LV1B        2
#define T_LV2         3
#define T_LV3         4
#define T_LV4         5
#define T_LV5A        6
#define T_LV5B        7
#define T_LV5C        8
#define T_LV6         9
#define T_LV7A        10
#define T_LV7B        11
#define T_LV7C        12
#define T_LV7D        13
#define T_LV8         14
#define T_LV9         15
#define T_LV10        16
#define T_LV11        17
#define T_LV13        18
#define T_MINE        19
#define T_WALL        20
#define T_CHEST_HEAL  21
#define T_CHEST_EYE2  22
#define T_CHEST_EXP   23
// items:
#define T_ITEM_HEAL   24
#define T_ITEM_EYE    25
#define T_ITEM_EYE2   26
#define T_ITEM_SHOW1  27
#define T_ITEM_SHOW5  28
#define T_ITEM_EXP1   29
#define T_ITEM_EXP3   30
#define T_ITEM_EXP5   31
#define T_ITEM_EXIT   32
// max of 63

// tile status
#define S_HIDDEN      0
#define S_VISIBLE     1
#define S_PRESSED     2
#define S_KILLED      3

#define GET_TYPE(b)        ((b) & 0x3f)
#define SET_TYPE(b, s)     b = ((b) & 0xc0) | (s)
#define GET_STATUS(b)      (((b) >> 6) & 3)
#define SET_STATUS(b, s)   b = GET_TYPE(b) | ((s) << 6)
#define IS_CHEST(b)        (GET_TYPE(b) >= T_CHEST_HEAL && GET_TYPE(b) <= T_CHEST_EXP)
#define IS_ITEM(b)         (GET_TYPE(b) >= T_ITEM_HEAL && GET_TYPE(b) <= T_ITEM_EXIT)

enum game_event {
  EV_TILE_UPDATE, // (x, y) tile location
  EV_HP_UPDATE,   // (hp, max_hp)
  EV_EXP_UPDATE,  // (exp, max_exp)
  EV_YOU_LOSE,    // (x, y) tile that killed you
  EV_YOU_WIN,
  EV_WAIT         // wait x number of frames
};

typedef void (*game_handler_f)(struct game_st *game, enum game_event ev, i32 x, i32 y);

static inline void levelgen_seed(struct levelgen_st *ctx, u32 seed) {
  rnd_seed(&ctx->rnd, seed);
}

void levelgen_stage1(struct levelgen_st *ctx);
void levelgen_stage2(struct levelgen_st *ctx);
void levelgen_onlymines(struct levelgen_st *ctx, u32 difficulty);

void game_new(struct game_st *game, struct levelgen_st *ctx);
void game_hover(struct game_st *game, game_handler_f handler, i32 x, i32 y);
bool game_click(struct game_st *game, game_handler_f handler);
bool game_levelup(struct game_st *game, game_handler_f handler);
void game_note(struct game_st *game, game_handler_f handler, i8 note);
i32 game_tileicon(u32 type);
i32 max_hp(struct game_st *game);
i32 max_exp(struct game_st *game);
i32 count_threat(const u8 *board, i32 x, i32 y);

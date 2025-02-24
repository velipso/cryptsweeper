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
#include <stdbool.h>
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

#define GENERATE_SIZE  128

struct game_st {
  struct rnd_st rnd;
  u32 totalexp;
  i8 selx;
  i8 sely;
  u8 difficulty;
  u8 level;
  u8 hp;
  u8 exp;
  u8 win; // 0 = play, 1 = dead, 2 = win
  struct {
    i8 size;
    u8 x[4];
    u8 y[4];
  } slimeking;
  u8 losthp;
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

// difficulty flags
#define D_DIFFICULTY   0x0f
#define D_ONLYMINES    0x10

// tile types
enum game_type {
  T_EMPTY     ,
  T_LAVA      ,
  T_LV1A      , /* spider */
  T_LV1B      , /* slimeking */
  T_LV2       ,
  T_LV3A      , /* normal */
  T_LV3B      , /* group 2 */
  T_LV3C      , /* group 3 */
  T_LV4A      , /* rook */
  T_LV4B      , /* bishop */
  T_LV4C      , /* knight */
  T_LV5A      , /* slime */
  T_LV5B      , /* spiderking */
  T_LV5C      , /* gazer */
  T_LV6       ,
  T_LV7       ,
  T_LV8       ,
  T_LV9       ,
  T_LV10      ,
  T_LV11      , /* mimic */
  T_LV13      ,
  T_MINE      ,
  T_WALL      ,
  // chests:
  T_CHEST_HEAL,
  T_CHEST_EYE2,
  T_CHEST_EXP ,
  // items:
  T_ITEM_HEAL ,
  T_ITEM_EYE  ,
  T_ITEM_EYE2 ,
  T_ITEM_SHOW1,
  T_ITEM_SHOW5,
  T_ITEM_EXP1 ,
  T_ITEM_EXP3 ,
  T_ITEM_EXP5 ,
  T_ITEM_EXP6 ,
  T_ITEM_EXP9 ,
  T_ITEM_LV3B0,
  T_ITEM_LV3C0,
  T_ITEM_LAVA ,
  T_ITEM_EXIT
  // max of 63
};

// tile status
#define S_HIDDEN      0
#define S_VISIBLE     1
#define S_PRESSED     2
#define S_KILLED      3

#define GET_TYPE(b)               ((enum game_type)((b) & 0x3f))
#define GET_TYPEXY(b, x, y)       GET_TYPE((b)[(x) + (y) * BOARD_W])
#define SET_TYPE(b, t)            b = ((b) & 0xc0) | (t)
#define SET_TYPEXY(b, x, y, t)    do { i32 k = (x) + (y) * BOARD_W; SET_TYPE(b[k], t); } while (0)
#define GET_STATUS(b)             (((b) >> 6) & 3)
#define GET_STATUSXY(b, x, y)     GET_STATUS((b)[(x) + (y) * BOARD_W])
#define SET_STATUS(b, s)          b = GET_TYPE(b) | ((s) << 6)
#define SET_STATUSXY(b, x, y, s)  do { i32 k = (x) + (y) * BOARD_W; SET_STATUS(b[k], s); } while (0)
#define IS_EMPTY(b)               (GET_TYPE(b) == T_EMPTY || GET_TYPE(b) == T_LAVA)
#define IS_EMPTYXY(b, x, y)       IS_EMPTY((b)[(x) + (y) * BOARD_W])
#define IS_MONSTER(b)             (GET_TYPE(b) >= T_LV1A && GET_TYPE(b) <= T_LV13)
#define IS_CHEST(b)               (GET_TYPE(b) >= T_CHEST_HEAL && GET_TYPE(b) <= T_CHEST_EXP)
#define IS_CHESTXY(b, x, y)       IS_CHEST((b)[(x) + (y) * BOARD_W])
#define IS_ITEM(b)                (GET_TYPE(b) >= T_ITEM_HEAL && GET_TYPE(b) <= T_ITEM_EXIT)
#define IS_ITEMXY(b, x, y)        IS_ITEM((b)[(x) + (y) * BOARD_W])

enum game_event {
  EV_PRESS_EMPTY, // (x, y) tile location where pressing empty
  EV_TILE_UPDATE, // (x, y) tile location
  EV_HP_UPDATE,   // (hp, max_hp)
  EV_EXP_UPDATE,  // (exp, max_exp)
  EV_SHOW_LAVA,
  EV_HOVER_LAVA,
  EV_YOU_LOSE,    // (x, y) tile that killed you
  EV_YOU_WIN,
  EV_WAIT,        // wait x number of frames
  EV_DEBUGLOG     // (id, value)
};

typedef void (*game_handler_f)(struct game_st *game, enum game_event ev, i32 x, i32 y);

void game_new(struct game_st *game, i32 difficulty, u32 seed, const u8 *board);
void game_hover(struct game_st *game, game_handler_f handler, i32 x, i32 y);
bool game_click(struct game_st *game, game_handler_f handler);
bool game_levelup(struct game_st *game, game_handler_f handler);
void game_note(struct game_st *game, game_handler_f handler, i8 note);
i32 game_tileicon(u32 type);
i32 max_hp(struct game_st *game);
bool next_level_hp_increases(struct game_st *game);
i32 max_exp(struct game_st *game);
i32 count_threat(const u8 *board, i32 x, i32 y);
i32 game_hint(struct game_st *game, game_handler_f handler, i32 knowledge);
// byte 0: x, byte 1: y, byte 2: action (click, note, levelup, giveup), byte 3: note value

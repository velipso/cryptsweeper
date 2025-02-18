//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

//
// This library is stand-alone so it can be called from either the GBA or xform at compile-time
//

#include <stdint.h>

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

// entity types
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
#define T_CHEST_EYE   22
#define T_CHEST_EXP   23
// items:
#define T_ITEM_HEAL   24
#define T_ITEM_EYE    25
#define T_ITEM_SHOW1  26
#define T_ITEM_SHOW5  27
#define T_ITEM_EXP1   28
#define T_ITEM_EXP3   29
#define T_ITEM_EXP5   30
// max of 63

#define GET_TYPE(b)        ((b) & 0x3f)
#define SET_TYPE(b, s)     b = ((b) & 0xc0) | (s)
#define GET_STATUS(b)      (((b) >> 6) & 3)
#define SET_STATUS(b, s)   b = GET_TYPE(b) | ((s) << 6)
#define IS_ITEM(b)         (GET_TYPE(b) >= T_ITEM_HEAL)

struct levelgen_ctx {
  u32 rnd_seed;
  u32 rnd_i;
  u32 unfixed_size;
  u8 board_order[BOARD_SIZE];
  u8 board[BOARD_SIZE];
  u8 unfixed[BOARD_SIZE];
};

static inline void levelgen_seed(struct levelgen_ctx *ctx, u32 seed) {
  ctx->rnd_seed = seed;
  ctx->rnd_i = 1;
}

void levelgen_stage1(struct levelgen_ctx *ctx);
void levelgen_stage2(struct levelgen_ctx *ctx);

//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#include <stdint.h>
#include "../src/game.h"

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;

void print_board(const u8 *board);
void generate_onlymines(u8 *board, i32 diff, struct rnd_st *rnd);
void generate_normal(u8 *board, i32 diff, struct rnd_st *rnd);

//
// cryptsweeper - fight the graveyard monsters and stop death
// by Pocket Pulp (@velipso), https://pulp.biz
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

//
// This library is stand-alone so it can be called from either the GBA or xform at compile-time
//

#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;

struct rnd_st {
  u32 seed;
  u32 i;
};

static inline u32 whisky2(u32 i0, u32 i1){
  u32 z0 = (i1 * 1833778363) ^ i0;
  u32 z1 = (z0 *  337170863) ^ (z0 >> 13) ^ z0;
  u32 z2 = (z1 *  620363059) ^ (z1 >> 10);
  u32 z3 = (z2 *  232140641) ^ (z2 >> 21);
  return z3;
}

static inline void rnd_seed(struct rnd_st *ctx, u32 seed) {
  ctx->seed = seed;
  ctx->i = 1;
}

static inline u32 rnd32(struct rnd_st *ctx) {
  return whisky2(ctx->seed, ++ctx->i);
}

//
// index = 0, true 100% of the time
// index = 1, true 50% of the time
// index = 2, true 33% of the time
// generally, true 1/(index + 1) of the time
// max index of 255
//
// useful for picking a random item from a list where you don't know the list length ahead of time
// so you can do something like this:
//
//   u32 index = 0;
//   chosen_item = NULL;
//   while (1) {
//     item = fetch_next_item(); // expensive
//     if (!item) break; // no more items!
//     if (random_pick(index, seed)) {
//       chosen_item = item;
//     }
//     index++;
//   }
//   chosen_item is a random element uniformly distributed
//
bool rnd_pick(struct rnd_st *ctx, u32 index);

// random number from 0 to sides-1
u32 roll(struct rnd_st *ctx, u32 sides);

// shuffles a u8[]
void shuffle8(struct rnd_st *ctx, u8 *arr, u32 length);

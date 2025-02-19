//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#pragma once
#include "sys.h"

extern const u8 ctz32_bitpos[32];
extern const u16 inverse16_lut[256];

static inline u32 ctz32(u32 v) {
  if (v == 0) return 32;
  return ctz32_bitpos[((v & -v) * 0x077cb531) >> 27];
}

// 1/n in Q1.15.16
static inline i32 inverse16(i32 n) {
  if (n == 0) return 0;
  if (n == 1) return 0x10000;
  if (n == -1) return -0x10000;
  if (n < 0) {
    return -inverse16_lut[n <= -255 ? 255 : -n];
  } else {
    return inverse16_lut[n > 255 ? 255 : n];
  }
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
bool random_pick(u32 index, u32 seed);

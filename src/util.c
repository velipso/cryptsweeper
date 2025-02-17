//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#include "sys.h"
#include "util.h"
#include "main.h"

const u8 ctz32_bitpos[32] SECTION_ROM = {
  0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 31, 27, 13, 23, 21, 19, 16, 7, 26, 12,
  18, 6, 11, 5, 10, 9
};

u32 roll(u32 sides) { // returns random number from 0 to sides-1
  switch (sides) {
    case 0: return 0;
    case 1: return 0;
    case 2: return rnd32() & 1;
    case 3: {
      u32 r = rnd32() & 3;
      while (r == 3) r = rnd32() & 3;
      return r;
    }
    case 4: return rnd32() & 3;
  }
  u32 mask = sides - 1;
  mask |= mask >> 1;
  mask |= mask >> 2;
  mask |= mask >> 4;
  mask |= mask >> 8;
  mask |= mask >> 16;
  u32 r = rnd32() & mask;
  while (r >= sides) {
    r = rnd32() & mask;
  }
  return r;
}

void shuffle32(u32 *arr, u32 length) {
  for (u32 i = length - 1; i > 0; i--) {
    u32 r = roll(i + 1);
    u32 temp = arr[i];
    arr[i] = arr[r];
    arr[r] = temp;
  }
}

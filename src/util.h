//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

extern const u8 ctz32_bitpos[32];

static inline u32 ctz32(u32 v) {
  if (v == 0) return 32;
  return ctz32_bitpos[((v & -v) * 0x077cb531) >> 27];
}

u32 roll(u32 sides); // returns random number from 0 to sides-1
void shuffle32(u32 *arr, u32 length);

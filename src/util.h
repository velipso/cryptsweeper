//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

extern const u8 ctz32_bitpos[32];
extern const u16 inverse_lut[256];

static inline u32 ctz32(u32 v) {
  if (v == 0) return 32;
  return ctz32_bitpos[((v & -v) * 0x077cb531) >> 27];
}

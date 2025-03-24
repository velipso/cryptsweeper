//
// cryptsweeper - fight the graveyard monsters and stop death
// by Pocket Pulp (@velipso), https://pulp.biz
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#include "anidata.h"
#include <stdlib.h>

#define U2(x) \
  __builtin_choose_expr(__builtin_constant_p(x) && \
    (x) >= 0 && (x) <= 3, (x), \
  exit(1))
#define U4(x) \
  __builtin_choose_expr(__builtin_constant_p(x) && \
    (x) >= 0 && (x) <= 15, (x), \
  exit(1))
#define U8(x) \
  __builtin_choose_expr(__builtin_constant_p(x) && \
    (x) >= 0 && (x) <= 255, (x), \
  exit(1))
#define S8(x) \
  __builtin_choose_expr(__builtin_constant_p(x) && \
    (x) >= -128 && (x) <= 127, (x) < 0 ? (x) + 0x100 : (x), \
  exit(1))
#define U10(x) \
  __builtin_choose_expr(__builtin_constant_p(x) && \
    (x) >= 0 && (x) <= 1023, (x), \
  exit(1))
#define U12(x) \
  __builtin_choose_expr(__builtin_constant_p(x) && \
    (x) >= 0 && (x) <= 4095, (x), \
  exit(1))
#define S12(x) \
  __builtin_choose_expr(__builtin_constant_p(x) && \
    (x) >= -2048 && (x) <= 2047, (x) < 0 ? (x) + 0x1000 : (x), \
  exit(1))
#define F48(x)  S12(((int)((x) * 256)))
#define F84(x)  S12(((int)((x) * 16)))

#define RESET()        0x0000
#define STOP()         0x0001
#define DESTROY()      0x0002
#define SOFTRESET()    0x0003
#define SIZE(v)        (0x0010 | U4(v))
#define SIZE_8x8()     SIZE(0)
#define SIZE_16x16()   SIZE(1)
#define SIZE_32x32()   SIZE(2)
#define SIZE_64x64()   SIZE(3)
#define SIZE_16x8()    SIZE(4)
#define SIZE_32x8()    SIZE(5)
#define SIZE_32x16()   SIZE(6)
#define SIZE_64x32()   SIZE(7)
#define SIZE_8x16()    SIZE(8)
#define SIZE_8x32()    SIZE(9)
#define SIZE_16x32()   SIZE(10)
#define SIZE_32x64()   SIZE(11)
#define PRIORITY(v)    (0x0020 | U4(v))
#define PALETTE(v)     (0x0030 | U4(v))
#define FLIP(v)        (0x0040 | U2(v))
#define BLEND(v)       (0x0050 | U2(v))
#define WAIT(v)        (0x0100 | U8(v))
#define LOOP(v)        (0x0200 | U8(v))
#define JUMP(v)        (0x0300 | S8(v))
#define JUMPIFLOOP(v)  (0x0400 | S8(v))
#define ADDRNDX(v)     (0x0500 | S8(v))
#define ADDRNDY(v)     (0x0600 | S8(v))
#define ADDRNDDX(v)    (0x0700 | S8(v))
#define ADDRNDDY(v)    (0x0800 | S8(v))
#define SPAWN(v)       (0x0900 | U8(v))
#define ADDTILE(v)     (0x0a00 | S8(v))
#define TILE(v)        (0x1000 | U10(v))
#define TILEXY(x, y)   TILE((((x) >> 3) + (((y) >> 3) << 4)) << 1)
#define GRAVITY(v)     (0x4000 | F48(v))
#define X(v)           (0x5000 | F84(v))
#define Y(v)           (0x6000 | F84(v))
#define ADDX(v)        (0x7000 | F84(v))
#define ADDY(v)        (0x8000 | F84(v))
#define DX(v)          (0x9000 | F84(v))
#define DY(v)          (0xa000 | F84(v))
#define ADDDX(v)       (0xb000 | F84(v))
#define ADDDY(v)       (0xc000 | F84(v))

#define CW1  10
#define CW2  8
#define CW3  5
#define CW4  8

const u16 ani_cursor1[] ={
  RESET(),
  SIZE_8x8(),
  TILEXY(0, 0),
  X(-2),
  Y(-2),
  WAIT(CW1),
  ADDX(-1),
  ADDY(-1),
  WAIT(CW2),
  ADDX(-1),
  ADDY(-1),
  WAIT(CW3),
  ADDX(1),
  ADDY(1),
  WAIT(CW4),
  ADDX(1),
  ADDY(1),
  JUMP(-12),
  STOP()
};
const u16 ani_cursor2[] ={
  RESET(),
  SIZE_8x8(),
  TILEXY(8, 0),
  X(-6),
  Y(-2),
  WAIT(CW1),
  ADDX(1),
  ADDY(-1),
  WAIT(CW2),
  ADDX(1),
  ADDY(-1),
  WAIT(CW3),
  ADDX(-1),
  ADDY(1),
  WAIT(CW4),
  ADDX(-1),
  ADDY(1),
  JUMP(-12),
  STOP()
};
const u16 ani_cursor3[] ={
  RESET(),
  SIZE_8x8(),
  TILEXY(0, 8),
  X(-2),
  Y(-6),
  WAIT(CW1),
  ADDX(-1),
  ADDY(1),
  WAIT(CW2),
  ADDX(-1),
  ADDY(1),
  WAIT(CW3),
  ADDX(1),
  ADDY(-1),
  WAIT(CW4),
  ADDX(1),
  ADDY(-1),
  JUMP(-12),
  STOP()
};
const u16 ani_cursor4[] ={
  RESET(),
  SIZE_8x8(),
  TILEXY(8, 8),
  X(-6),
  Y(-6),
  WAIT(CW1),
  ADDX(1),
  ADDY(1),
  WAIT(CW2),
  ADDX(1),
  ADDY(1),
  WAIT(CW3),
  ADDX(-1),
  ADDY(-1),
  WAIT(CW4),
  ADDX(-1),
  ADDY(-1),
  JUMP(-12),
  STOP()
};

const u16 ani_cursor1_pause[] ={
  RESET(),
  SIZE_8x8(),
  TILEXY(0, 0),
  X(-2),
  Y(-2),
  STOP()
};
const u16 ani_cursor2_pause[] ={
  RESET(),
  SIZE_8x8(),
  TILEXY(8, 0),
  X(-6),
  Y(-2),
  STOP()
};
const u16 ani_cursor3_pause[] ={
  RESET(),
  SIZE_8x8(),
  TILEXY(0, 8),
  X(-2),
  Y(-6),
  STOP()
};
const u16 ani_cursor4_pause[] ={
  RESET(),
  SIZE_8x8(),
  TILEXY(8, 8),
  X(-6),
  Y(-6),
  STOP()
};

const u16 ani_hpfull[] = {
  RESET(),
  SIZE_8x8(),
  TILEXY(0, 16),
  STOP()
};
const u16 ani_hpempty[] = {
  RESET(),
  SIZE_8x8(),
  TILEXY(8, 16),
  STOP()
};
const u16 ani_hphalf[] = {
  RESET(),
  SIZE_8x8(),
  TILEXY(96, 32),
  STOP()
};
const u16 ani_hp0[] = {
  RESET(),
  SIZE_8x16(),
  TILEXY(16, 0),
  STOP()
};
const u16 ani_hp1[] = {
  RESET(),
  SIZE_8x16(),
  TILEXY(24, 0),
  STOP()
};
const u16 ani_hp2[] = {
  RESET(),
  SIZE_8x16(),
  TILEXY(32, 0),
  STOP()
};
const u16 ani_hp3[] = {
  RESET(),
  SIZE_8x16(),
  TILEXY(40, 0),
  STOP()
};
const u16 ani_hp4[] = {
  RESET(),
  SIZE_8x16(),
  TILEXY(48, 0),
  STOP()
};
const u16 ani_hp5[] = {
  RESET(),
  SIZE_8x16(),
  TILEXY(56, 0),
  STOP()
};
const u16 ani_hp6[] = {
  RESET(),
  SIZE_8x16(),
  TILEXY(64, 0),
  STOP()
};
const u16 ani_hp7[] = {
  RESET(),
  SIZE_8x16(),
  TILEXY(72, 0),
  STOP()
};
const u16 ani_hp8[] = {
  RESET(),
  SIZE_8x16(),
  TILEXY(80, 0),
  STOP()
};
const u16 ani_hp9[] = {
  RESET(),
  SIZE_8x16(),
  TILEXY(88, 0),
  STOP()
};
const u16 ani_hpslash[] = {
  RESET(),
  SIZE_8x16(),
  TILEXY(96, 0),
  STOP()
};
const u16 *ani_hpnum[] = {
  ani_hp0,
  ani_hp1,
  ani_hp2,
  ani_hp3,
  ani_hp4,
  ani_hp5,
  ani_hp6,
  ani_hp7,
  ani_hp8,
  ani_hp9,
  ani_hpslash,
};

const u16 ani_expfull[] ={
  RESET(),
  SIZE_8x8(),
  TILEXY(0, 24),
  STOP()
};
const u16 ani_expempty[] ={
  RESET(),
  SIZE_8x8(),
  TILEXY(8, 24),
  STOP()
};
const u16 ani_exp0[] ={
  RESET(),
  SIZE_8x16(),
  TILEXY(16, 16),
  STOP()
};
const u16 ani_exp1[] ={
  RESET(),
  SIZE_8x16(),
  TILEXY(24, 16),
  STOP()
};
const u16 ani_exp2[] ={
  RESET(),
  SIZE_8x16(),
  TILEXY(32, 16),
  STOP()
};
const u16 ani_exp3[] ={
  RESET(),
  SIZE_8x16(),
  TILEXY(40, 16),
  STOP()
};
const u16 ani_exp4[] ={
  RESET(),
  SIZE_8x16(),
  TILEXY(48, 16),
  STOP()
};
const u16 ani_exp5[] ={
  RESET(),
  SIZE_8x16(),
  TILEXY(56, 16),
  STOP()
};
const u16 ani_exp6[] ={
  RESET(),
  SIZE_8x16(),
  TILEXY(64, 16),
  STOP()
};
const u16 ani_exp7[] ={
  RESET(),
  SIZE_8x16(),
  TILEXY(72, 16),
  STOP()
};
const u16 ani_exp8[] ={
  RESET(),
  SIZE_8x16(),
  TILEXY(80, 16),
  STOP()
};
const u16 ani_exp9[] ={
  RESET(),
  SIZE_8x16(),
  TILEXY(88, 16),
  STOP()
};
const u16 ani_expslash[] ={
  RESET(),
  SIZE_8x16(),
  TILEXY(96, 16),
  STOP()
};
const u16 *ani_expnum[] = {
  ani_exp0,
  ani_exp1,
  ani_exp2,
  ani_exp3,
  ani_exp4,
  ani_exp5,
  ani_exp6,
  ani_exp7,
  ani_exp8,
  ani_exp9,
  ani_expslash,
};

const u16 ani_expselect1[] = {
  RESET(),
  SIZE_32x16(),
  TILEXY(0, 32),
  WAIT(105),
  TILEXY(48, 32),
  WAIT(15),
  JUMP(-4),
  STOP()
};
const u16 ani_expselect2[] = {
  RESET(),
  SIZE_16x16(),
  X(32),
  TILEXY(32, 32),
  WAIT(105),
  TILEXY(80, 32),
  WAIT(15),
  JUMP(-4),
  STOP()
};

// generated via `node design/levelup.js`
#define EXPIWAIT(n)  \
  ADDX(-32),ADDY(36),WAIT(n*3),ADDX(8),ADDY(1),WAIT(1),ADDX(8),ADDY(1),WAIT(1),ADDX(8),WAIT(1),  \
  ADDX(8),WAIT(1),ADDX(8),ADDY(-1),WAIT(1),ADDX(7),ADDY(-1),WAIT(1),ADDX(7),ADDY(-2),WAIT(1),    \
  ADDX(7),ADDY(-2),WAIT(1),ADDX(6),ADDY(-2),WAIT(1),ADDX(6),ADDY(-2),WAIT(1),ADDX(5),ADDY(-3),   \
  WAIT(1),ADDX(4),ADDY(-3),WAIT(1),ADDX(3),ADDY(-3),WAIT(1),ADDX(3),ADDY(-3),WAIT(1),ADDX(2),    \
  ADDY(-3),WAIT(1),ADDX(2),ADDY(-3),WAIT(1),ADDX(1),ADDY(-4),WAIT(1),ADDY(-3),WAIT(1),ADDX(-1),  \
  ADDY(-3),WAIT(1),ADDX(-1),ADDY(-3),WAIT(1),ADDX(-2),ADDY(-3),WAIT(1),ADDX(-3),ADDY(-3),        \
  WAIT(1),ADDX(-3),ADDY(-2),WAIT(1),ADDX(-3),ADDY(-3),WAIT(1),ADDX(-4),ADDY(-2),WAIT(1),         \
  ADDX(-5),ADDY(-2),WAIT(1),ADDX(-4),ADDY(-1),WAIT(1),ADDX(-5),ADDY(-2),WAIT(1),ADDX(-5),        \
  ADDY(-1),WAIT(1),ADDX(-5),WAIT(1),ADDX(-5),ADDY(-1),WAIT(1),ADDX(-5),WAIT(1),ADDX(-5),         \
  WAIT(1),ADDX(-5),ADDY(1),WAIT(1),ADDX(-5),WAIT(1),ADDX(-4),ADDY(1),WAIT(1),ADDX(-4),ADDY(1),   \
  WAIT(1),ADDX(-4),ADDY(1),WAIT(1),ADDX(-3),ADDY(2),WAIT(1),ADDX(-3),ADDY(1),WAIT(1),ADDX(-2),   \
  ADDY(2),WAIT(1),ADDX(-2),ADDY(2),WAIT(1),ADDX(-2),ADDY(1),WAIT(1),ADDX(-1),ADDY(2),WAIT(1),    \
  ADDX(-1),ADDY(2),WAIT(1),ADDY(2),WAIT(1),ADDY(2),WAIT(1),ADDY(1),WAIT(1),ADDX(1),ADDY(2),      \
  WAIT(1),ADDX(1),ADDY(2),WAIT(1),ADDX(2),ADDY(1),WAIT(1),ADDX(1),ADDY(1),WAIT(1),ADDX(2),       \
  ADDY(1),WAIT(1),ADDX(2),ADDY(1),WAIT(1),ADDX(3),ADDY(1),WAIT(1),ADDX(2),ADDY(1),WAIT(1),       \
  ADDX(2),WAIT(1),ADDX(3),WAIT(1),ADDX(2),ADDY(1),WAIT(1),ADDX(2),WAIT(1),ADDX(2),WAIT(1),       \
  ADDX(2),ADDY(-1),WAIT(1),ADDX(2),WAIT(1),ADDX(2),WAIT(1),ADDX(1),ADDY(-1),WAIT(1),ADDX(2),     \
  ADDY(-1),WAIT(1),ADDX(1),WAIT(1),ADDY(-1),WAIT(1),ADDX(1),ADDY(-1),WAIT(1),WAIT(1),ADDY(-1),   \
  WAIT(1),ADDY(-1),WAIT(1),WAIT(1),ADDX(-1),ADDY(-1),WAIT(1),WAIT(1),ADDX(-1),ADDY(-1),WAIT(1),  \
  ADDX(-1),WAIT(1),ADDX(-1),WAIT(1),ADDX(-1),WAIT(1),WAIT(240),ADDY(-2),ADDX(1),WAIT(1),         \
  ADDY(-2),WAIT(1),ADDY(-1),ADDX(1),WAIT(1),ADDY(-1),WAIT(1),WAIT(1),WAIT(1),ADDY(1),ADDX(-1),   \
  WAIT(1),ADDY(1),WAIT(1),ADDY(2),ADDX(-1),WAIT(1),ADDY(2),WAIT(1),WAIT(240),JUMP(-24)

const u16 ani_expiL[] = {
  RESET(),
  SIZE_8x8(),
  TILEXY(0, 48),
  EXPIWAIT(0),
  STOP()
};
const u16 ani_expiE[] = {
  RESET(),
  SIZE_8x8(),
  TILEXY(8, 48),
  EXPIWAIT(1),
  STOP()
};
const u16 ani_expiV[] = {
  RESET(),
  SIZE_8x8(),
  TILEXY(16, 48),
  EXPIWAIT(2),
  STOP()
};
const u16 ani_expiE2[] = {
  RESET(),
  SIZE_8x8(),
  TILEXY(8, 48),
  EXPIWAIT(3),
  STOP()
};
const u16 ani_expiL2[] = {
  RESET(),
  SIZE_8x8(),
  TILEXY(0, 48),
  EXPIWAIT(4),
  STOP()
};
const u16 ani_expiU[] = {
  RESET(),
  SIZE_8x8(),
  TILEXY(24, 48),
  EXPIWAIT(5),
  STOP()
};
const u16 ani_expiP[] = {
  RESET(),
  SIZE_8x8(),
  TILEXY(32, 48),
  EXPIWAIT(6),
  STOP()
};
const u16 ani_expiX[] = {
  RESET(),
  SIZE_8x8(),
  TILEXY(40, 48),
  EXPIWAIT(7),
  STOP()
};
const u16 ani_expiX2[] = {
  RESET(),
  SIZE_8x8(),
  TILEXY(40, 48),
  EXPIWAIT(8),
  STOP()
};
const u16 ani_expiX3[] = {
  RESET(),
  SIZE_8x8(),
  TILEXY(40, 48),
  EXPIWAIT(9),
  STOP()
};

const u16 ani_note[] = {
  RESET(),
  SIZE_64x64(),
  TILEXY(0, 64),
  STOP(),
};
const u16 ani_notecur[] = {
  RESET(),
  SIZE_32x16(),
  TILEXY(48, 48),
  STOP(),
};

const u16 ani_popup[] = {
  RESET(),
  SIZE_64x64(),
  TILEXY(0, 128),
  STOP(),
};
const u16 ani_arrowr[] = {
  RESET(),
  SIZE_8x16(),
  TILEXY(64, 176),
  WAIT(10),
  ADDX(1),
  WAIT(10),
  ADDX(-1),
  JUMP(-4),
  STOP()
};
const u16 ani_arrowr2[] = {
  RESET(),
  SIZE_8x16(),
  TILEXY(80, 176),
  WAIT(10),
  ADDX(1),
  WAIT(10),
  ADDX(-1),
  JUMP(-4),
  STOP()
};

const u16 ani_title_continue1[] = {
  RESET(),
  SIZE_32x16(),
  TILEXY(64, 128),
  STOP()
};
const u16 ani_title_continue2[] = {
  RESET(),
  SIZE_32x16(),
  TILEXY(96, 128),
  X(32),
  STOP()
};
const u16 ani_title_newgame1[] = {
  RESET(),
  SIZE_32x16(),
  TILEXY(64, 144),
  STOP()
};
const u16 ani_title_newgame2[] = {
  RESET(),
  SIZE_32x16(),
  TILEXY(96, 144),
  X(32),
  STOP()
};
const u16 ani_title_delete1[] = {
  RESET(),
  SIZE_32x16(),
  TILEXY(64, 160),
  STOP()
};
const u16 ani_title_delete2[] = {
  RESET(),
  SIZE_32x16(),
  TILEXY(96, 160),
  X(32),
  STOP()
};

const u16 ani_lv1b_f1[] = {
  RESET(),
  SIZE_32x32(),
  TILEXY(0, 192),
  PRIORITY(1),
  X(-8),
  Y(-8),
  STOP()
};
const u16 ani_lv1b_f2[] = {
  RESET(),
  SIZE_32x32(),
  TILEXY(64, 192),
  PRIORITY(1),
  X(-8),
  Y(-8),
  STOP()
};
const u16 ani_lv5b_f1[] = {
  RESET(),
  SIZE_32x32(),
  TILEXY(32, 192),
  PRIORITY(1),
  X(-8),
  Y(-8),
  STOP()
};
const u16 ani_lv5b_f2[] = {
  RESET(),
  SIZE_32x32(),
  TILEXY(96, 192),
  PRIORITY(1),
  X(-8),
  Y(-8),
  STOP()
};
const u16 ani_lv10_f1[] = {
  RESET(),
  SIZE_32x32(),
  TILEXY(0, 224),
  PRIORITY(1),
  X(-8),
  Y(-8),
  STOP()
};
const u16 ani_lv10_f2[] = {
  RESET(),
  SIZE_32x32(),
  TILEXY(64, 224),
  PRIORITY(1),
  X(-8),
  Y(-8),
  STOP()
};
const u16 ani_lv13_f1[] = {
  RESET(),
  SIZE_32x32(),
  TILEXY(32, 224),
  PRIORITY(1),
  X(-8),
  Y(-8),
  STOP()
};
const u16 ani_lv13_f2[] = {
  RESET(),
  SIZE_32x32(),
  TILEXY(96, 224),
  PRIORITY(1),
  X(-8),
  Y(-8),
  STOP()
};

const u16 ani_gray1[] = {
  SOFTRESET(),
  SIZE_8x8(),
  TILEXY(104, 0),
  PRIORITY(1),
  WAIT(25),
  DESTROY()
};
const u16 ani_gray2[] = {
  SOFTRESET(),
  SIZE_8x8(),
  TILEXY(112, 0),
  PRIORITY(1),
  WAIT(25),
  DESTROY()
};
const u16 ani_explodeL[] = {
  SOFTRESET(),
  SIZE_8x8(),
  TILEXY(112, 16),
  PRIORITY(1),
  WAIT(30),
  TILEXY(112, 24),
  WAIT(10),
  TILEXY(112, 32),
  WAIT(10),
  TILEXY(112, 40),
  WAIT(10),
  DESTROY()
};
const u16 ani_explodeR[] = {
  SOFTRESET(),
  SIZE_8x8(),
  TILEXY(120, 16),
  PRIORITY(1),
  WAIT(30),
  TILEXY(120, 24),
  WAIT(10),
  TILEXY(120, 32),
  WAIT(10),
  TILEXY(120, 40),
  WAIT(10),
  DESTROY()
};

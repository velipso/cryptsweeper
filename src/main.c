//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#include "main.h"
#include <stdlib.h>
#include "ani.h"
#include "anidata.h"
#include "sfx.h"
#include "util.h"
#include "levelgen.h"

#define S_CURSOR1  0
#define S_CURSOR2  1
#define S_CURSOR3  2
#define S_CURSOR4  3

#define B_SHOW     (1 << 8)
#define B_PRESSED  (1 << 9)

static u32 g_down;
static u32 g_hit;
static i32 g_cursor_x;
static i32 g_cursor_y;
static i32 g_seltile_x;
static i32 g_seltile_y;
static i32 g_brightness = 2;
static u32 g_board[BOARD_SIZE];
// .... .... .... ....  .... ..PS TTTT TTTT
//                             || \_______/
//                             ||     +----- type (0-255)
//                             |+----------- show answer player?
//                             +------------ pressed?
u32 rnd_seed = 1;
u32 rnd_i = 1;

static void SECTION_IWRAM_ARM irq_vblank() {
  sys_copy_oam(g_oam);
  u32 inp = sys_input() ^ 0x3ff;
  g_hit = ~g_down & inp;
  g_down = inp;
  rnd_seed = whisky2(rnd_seed, inp);
}

static void nextframe() {
  for (int i = 0; i < 128; i++)
    ani_step(i);
  sys_nextframe();
}

static u32 board_type_to_tile(u32 type) {
  #define TILE(px, py)  (px >> 3) + ((py >> 3) << 5)
  switch (type) {
    case T_EMPTY:      return 0;
    case T_EYE:        return TILE(64, 48);
    case T_LV1A:       return TILE(128, 0);
    case T_LV1B:       return TILE(128, 16);
    case T_LV2:        return TILE(128, 32);
    case T_LV3:        return TILE(128, 48);
    case T_LV4:        return TILE(128, 64);
    case T_LV5A:       return TILE(128, 80);
    case T_LV5B:       return TILE(128, 96);
    case T_LV5C:       return TILE(128, 112);
    case T_LV6:        return TILE(192, 0);
    case T_LV7A:       return TILE(192, 16);
    case T_LV7B:       return TILE(192, 16);
    case T_LV7C:       return TILE(192, 16);
    case T_LV7D:       return TILE(192, 16);
    case T_LV8:        return TILE(192, 32);
    case T_LV9:        return TILE(192, 48);
    case T_LV10:       return TILE(192, 64);
    case T_LV11:       return TILE(192, 80);
    case T_LV13:       return TILE(192, 96);
    case T_MINE:       return TILE(64, 0);
    case T_WALL:       return TILE(64, 32);
    case T_CHEST_HEAL: return TILE(64, 16);
    case T_CHEST_EYE:  return TILE(64, 16);
    case T_CHEST_EXP:  return TILE(64, 16);
  }
  return 0;
  #undef TILE
}

// -3 = blank
// -2 = mine
// -1 = ?
// 0-99 = number
static void place_number(i32 x, i32 y, u32 style, i32 num) {
  u32 offset = (2 + x) * 2 + (2 + y) * 64;
  if (num < -2) {
    sys_set_map(0x1d, offset + 0, 0);
    sys_set_map(0x1d, offset + 2, 0);
  } else if (num == -2) {
    sys_set_map(0x1d, offset + 0, 6);
    sys_set_map(0x1d, offset + 2, 7);
  } else if (num < 10) {
    if (num < 10) {
      num = 66 + 32 * num;
    }
    sys_set_map(0x1d, offset + 0, style + num);
    sys_set_map(0x1d, offset + 2, style + num + 1);
  } else {
    if (num > 99) num = 99;
    u32 d1 =
      num < 20 ? 1 :
      num < 30 ? 2 :
      num < 40 ? 3 :
      num < 50 ? 4 :
      num < 60 ? 5 :
      num < 70 ? 6 :
      num < 80 ? 7 :
      num < 90 ? 8 : 9;
    u32 d2 = num - d1 * 10;
    d1 = 64 + 32 * d1;
    d2 = 65 + 32 * d2;
    sys_set_map(0x1d, offset + 0, style + d1);
    sys_set_map(0x1d, offset + 2, style + d2);
  }
}

static void place_minehint(i32 x, i32 y, i32 num) {
  u32 offset = (2 + x) * 2 + (3 + y) * 64;
  switch (num) {
    case 0: num = 0; break;
    case 1: num = 32; break;
    case 2: num = 2; break;
    case 3: num = 4; break;
    case 4: num = 36; break;
    default: num = 0; break;
  }
  sys_set_map(0x1d, offset + 0, num);
  sys_set_map(0x1d, offset + 2, num == 0 ? 0 : num + 1);
}

static void place_answer(i32 x, i32 y, i32 frame) {
  frame *= 2;
  u32 t = board_type_to_tile(g_board[x + y * BOARD_W] & 0xff);
  u32 offset = (x + 1) * 4 + (y + 1) * 128;
  if (t == 0) {
    sys_set_map(0x1e, offset +  0, 0);
    sys_set_map(0x1e, offset +  2, 0);
    sys_set_map(0x1e, offset + 64, 0);
    sys_set_map(0x1e, offset + 66, 0);
  } else {
    sys_set_map(0x1e, offset +  0, frame + t +  0);
    sys_set_map(0x1e, offset +  2, frame + t +  1);
    sys_set_map(0x1e, offset + 64, frame + t + 32);
    sys_set_map(0x1e, offset + 66, frame + t + 33);
  }
}

static void place_unpressed(i32 x, i32 y) {
  u32 offset = (x + 1) * 4 + (y + 1) * 128;
  u32 r = rnd32() & 14;
  sys_set_map(0x1f, offset +  0, 384 + r);
  sys_set_map(0x1f, offset +  2, 385 + r);
  sys_set_map(0x1f, offset + 64, 416 + r);
  sys_set_map(0x1f, offset + 66, 417 + r);
}

static void place_pressed(i32 x, i32 y) {
  u32 offset = (x + 1) * 4 + (y + 1) * 128;
  u32 r = rnd32() & 14;
  sys_set_map(0x1f, offset +  0, 448 + r);
  sys_set_map(0x1f, offset +  2, 449 + r);
  sys_set_map(0x1f, offset + 64, 480 + r);
  sys_set_map(0x1f, offset + 66, 481 + r);
}

static void cursor_to_seltile() {
  g_cursor_x = g_seltile_x * 16 + 8;
  g_cursor_y = g_seltile_y * 16 + 3;
  g_sprites[S_CURSOR1].origin.x = g_cursor_x;
  g_sprites[S_CURSOR1].origin.y = g_cursor_y;
  g_sprites[S_CURSOR2].origin.x = g_cursor_x + 16;
  g_sprites[S_CURSOR2].origin.y = g_cursor_y;
  g_sprites[S_CURSOR3].origin.x = g_cursor_x;
  g_sprites[S_CURSOR3].origin.y = g_cursor_y + 16;
  g_sprites[S_CURSOR4].origin.x = g_cursor_x + 16;
  g_sprites[S_CURSOR4].origin.y = g_cursor_y + 16;
}

static void load_level(u32 seed) {
  struct levelgen_ctx *ctx = malloc(sizeof(struct levelgen_ctx));
  const u8 *levels = BINADDR(levels_bin);
  u32 base = whisky2(seed, 999) & ((BINSIZE(levels_bin) >> 7) - 1);
  levels += base * 128;
  for (i32 i = 0; i < BOARD_SIZE; i++) {
    ctx->board[i] = levels[i];
  }
  levelgen_seed(ctx, seed);
  //levelgen_stage2(ctx); // TODO: uncomment
  g_seltile_x = BOARD_CW;
  g_seltile_y = BOARD_CH;
  for (i32 y = 0; y < BOARD_H; y++) {
    for (i32 x = 0; x < BOARD_W; x++) {
      i32 i = x + y * BOARD_W;
      g_board[i] = ctx->board[i];
      if (g_board[i] == T_EYE) {
        g_board[i] |= B_SHOW;
        g_seltile_x = x;
        g_seltile_y = y;
      } else if (g_board[i] == T_LV13) {
        g_board[i] |= B_SHOW;
      }
    }
  }
  free(ctx);
  cursor_to_seltile();

  // clear board layers
  sys_set_bgt3_scroll(8, 13);
  sys_set_bgt2_scroll(8, 13);
  for (i32 y = -1; y <= BOARD_H; y++) {
    for (i32 x = -1; x <= BOARD_W; x++) {
      if (x < 0 || x >= BOARD_W || y < 0 || y >= BOARD_H) {
        // place border
        u32 offset = (x + 1) * 4 + (y + 1) * 128;
        sys_set_map(0x1f, offset +  0, 1);
        sys_set_map(0x1f, offset +  2, 1);
        sys_set_map(0x1f, offset + 64, 1);
        sys_set_map(0x1f, offset + 66, 1);
      } else {
        // place background tile
        place_unpressed(x, y);
        u32 t = g_board[x + y * BOARD_W];
        if (t & B_SHOW) {
          place_answer(x, y, 0);
        }
      }
    }
  }

  // clear number layer
  sys_set_bgt1_scroll(8, 10);
  for (i32 y = 0; y < BOARD_H; y++) {
    for (i32 x = 0; x < BOARD_W; x++) {
      place_number(x, y, 0, -3);
      place_minehint(x, y, 0);
    }
  }
}

void gvmain() {
  sys_init();
  snd_set_master_volume(16);
  snd_set_song_volume(8);
  snd_set_sfx_volume(8);
  snd_load_song(BINADDR(song1_gvsong), 1);
  sys_set_vblank(irq_vblank);
  gfx_setmode(GFX_MODE_4T);
  gfx_showbg0(true); // UI/pause
  sys_set_bg_config(
    0, // background #
    0, // priority
    2, // tile start
    0, // mosaic
    1, // 256 colors
    0x1c, // map start
    0, // wrap
    SYS_BGT_SIZE_256X256
  );
  gfx_showbg1(true); // numbers/marks
  sys_set_bg_config(
    1, // background #
    0, // priority
    0, // tile start
    0, // mosaic
    1, // 256 colors
    0x1d, // map start
    0, // wrap
    SYS_BGT_SIZE_256X256
  );
  gfx_showbg2(true); // board fg
  sys_set_bg_config(
    2, // background #
    0, // priority
    0, // tile start
    0, // mosaic
    1, // 256 colors
    0x1e, // map start
    0, // wrap
    SYS_BGT_SIZE_256X256
  );
  gfx_showbg3(true); // board bg
  sys_set_bg_config(
    3, // background #
    0, // priority
    0, // tile start
    0, // mosaic
    1, // 256 colors
    0x1f, // map start
    0, // wrap
    SYS_BGT_SIZE_256X256
  );
  gfx_showobj(true);
  sys_copy_tiles(0, 0, BINADDR(tiles_bin), BINSIZE(tiles_bin));
  sys_copy_tiles(2, 0, BINADDR(ui_bin), BINSIZE(ui_bin));
  sys_copy_tiles(4, 0, BINADDR(sprites_bin), BINSIZE(sprites_bin));
  sys_copy_bgpal(0, BINADDR(palette_brightness_bin) + g_brightness * 512, 512);
  sys_copy_spritepal(0, BINADDR(palette_brightness_bin) + g_brightness * 512, 512);
  gfx_showscreen(true);

  g_sprites[S_CURSOR1].pc = ani_cursor1;
  g_sprites[S_CURSOR2].pc = ani_cursor2;
  g_sprites[S_CURSOR3].pc = ani_cursor3;
  g_sprites[S_CURSOR4].pc = ani_cursor4;

  load_level(1234);

  for (;;) {
    nextframe();
    if (g_hit & SYS_INPUT_U) {
      if (g_seltile_y > 0) {
        g_seltile_y--;
        cursor_to_seltile();
      } else {
        sfx_bump();
      }
    } else if (g_hit & SYS_INPUT_R) {
      if (g_seltile_x < BOARD_W - 1) {
        g_seltile_x++;
        cursor_to_seltile();
      } else {
        sfx_bump();
      }
    } else if (g_hit & SYS_INPUT_D) {
      if (g_seltile_y < BOARD_H - 1) {
        g_seltile_y++;
        cursor_to_seltile();
      } else {
        sfx_bump();
      }
    } else if (g_hit & SYS_INPUT_L) {
      if (g_seltile_x > 0) {
        g_seltile_x--;
        cursor_to_seltile();
      } else {
        sfx_bump();
      }
    } else if (g_hit & SYS_INPUT_ST) {
      // TODO: slide up menu
    } else if (g_hit & SYS_INPUT_B) {
      // TODO: marker popup
    } else if (g_hit & SYS_INPUT_A) {
      u32 k = g_seltile_x + g_seltile_y * BOARD_W;
      if (g_board[k] & B_PRESSED) {
        switch (g_board[k] & 0xff) {
          case T_EMPTY: break;
          case T_EYE: break;
          case T_LV1A:
          case T_LV1B:
          case T_LV2:
          case T_LV3:
          case T_LV4:
          case T_LV5A:
          case T_LV5B:
          case T_LV5C:
          case T_LV6:
          case T_LV7A:
          case T_LV7B:
          case T_LV7C:
          case T_LV7D:
          case T_LV8:
          case T_LV9:
          case T_LV10:
          case T_LV11:
          case T_LV13:
            // TODO: fight monster
            break;
          case T_MINE:
            // TODO: die
            break;
          case T_WALL:
            // TODO: attack wall
            break;
          case T_CHEST_HEAL:
            // TODO: open chest
            break;
          case T_CHEST_EYE:
            // TODO: open chest
            break;
          case T_CHEST_EXP:
            // TODO: open chest
            break;
        }
      } else {
        // press tile
        g_board[k] |= B_PRESSED | B_SHOW;
        place_answer(g_seltile_x, g_seltile_y, 0);
        place_pressed(g_seltile_x, g_seltile_y);
        switch (g_board[k] & 0xff) {
          case T_EMPTY: break;
          case T_EYE:
            // TODO: expose squares
            break;
          case T_LV1A:
          case T_LV1B:
          case T_LV2:
          case T_LV3:
          case T_LV4:
          case T_LV5A:
          case T_LV5B:
          case T_LV5C:
          case T_LV6:
          case T_LV7A:
          case T_LV7B:
          case T_LV7C:
          case T_LV7D:
          case T_LV8:
          case T_LV9:
          case T_LV10:
          case T_LV11:
          case T_LV13:
            // TODO: fight monster
            break;
          case T_MINE:
            // TODO: die
            break;
          case T_WALL:
          case T_CHEST_HEAL:
          case T_CHEST_EYE:
          case T_CHEST_EXP:
            // do nothing
            break;
        }
      }
    }
  }
}

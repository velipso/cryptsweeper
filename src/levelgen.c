//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#include "levelgen.h"

static inline u32 whisky2(u32 i0, u32 i1){
  u32 z0 = (i1 * 1833778363) ^ i0;
  u32 z1 = (z0 *  337170863) ^ (z0 >> 13) ^ z0;
  u32 z2 = (z1 *  620363059) ^ (z1 >> 10);
  u32 z3 = (z2 *  232140641) ^ (z2 >> 21);
  return z3;
}

static inline u32 rnd32(struct levelgen_ctx *ctx) {
  return whisky2(ctx->rnd_seed, ++ctx->rnd_i);
}

static u32 roll(struct levelgen_ctx *ctx, u32 sides) { // returns random number from 0 to sides-1
  switch (sides) {
    case 0: return 0;
    case 1: return 0;
    case 2: return rnd32(ctx) & 1;
    case 3: {
      u32 r = rnd32(ctx) & 3;
      while (r == 3) r = rnd32(ctx) & 3;
      return r;
    }
    case 4: return rnd32(ctx) & 3;
  }
  u32 mask = sides - 1;
  mask |= mask >> 1;
  mask |= mask >> 2;
  mask |= mask >> 4;
  mask |= mask >> 8;
  mask |= mask >> 16;
  u32 r = rnd32(ctx) & mask;
  while (r >= sides) {
    r = rnd32(ctx) & mask;
  }
  return r;
}

static void shuffle8(struct levelgen_ctx *ctx, u8 *arr, u32 length) {
  for (u32 i = length - 1; i > 0; i--) {
    u32 r = roll(ctx, i + 1);
    u32 temp = arr[i];
    arr[i] = arr[r];
    arr[r] = temp;
  }
}

static i32 count_border(struct levelgen_ctx *ctx, i32 x, i32 y, u32 type) {
  i32 result = 0;
  #define CHECK(dx, dy)  do {                                   \
      if (ctx->board[(x + dx) + (y + dy) * BOARD_W] == type) {  \
        result++;                                               \
      }                                                         \
    } while (0)
  if (x > 0) {
    CHECK(-1, 0);
    if (y >           0) CHECK(-1, -1);
    if (y < BOARD_H - 1) CHECK(-1, 1);
  }
  if (y >           0) CHECK(0, -1);
  if (y < BOARD_H - 1) CHECK(0, 1);
  if (x < BOARD_W - 1) {
    CHECK(1, 0);
    if (y >           0) CHECK(1, -1);
    if (y < BOARD_H - 1) CHECK(1, 1);
  }
  return result;
  #undef CHECK
}

static i32 count_border_rad(struct levelgen_ctx *ctx, i32 x, i32 y, i32 rad, u32 type) {
  i32 result = 0;
  for (i32 dy = -rad; dy <= rad; dy++) {
    i32 by = dy + y;
    if (by < 0 || by >= BOARD_H) continue;
    by *= BOARD_W;
    for (i32 dx = -rad; dx <= rad; dx++) {
      if (dy == 0 && dx == 0) continue;
      i32 bx = dx + x;
      if (bx < 0 || bx >= BOARD_W) continue;
      if (ctx->board[bx + by] == type) {
        result++;
      }
    }
  }
  return result;
}

static i32 count_border_chest(struct levelgen_ctx *ctx, i32 x, i32 y) {
  i32 result = 0;
  for (i32 dy = -3; dy <= 3; dy++) {
    i32 by = dy + y;
    if (by < 0 || by >= BOARD_H) continue;
    by *= BOARD_W;
    for (i32 dx = -3; dx <= 3; dx++) {
      if (dy == 0 && dx == 0) continue;
      i32 bx = dx + x;
      if (bx < 0 || bx >= BOARD_W) continue;
      u8 type = ctx->board[bx + by];
      if (type == T_CHEST_HEAL || type == T_CHEST_EYE || type == T_CHEST_EXP) {
        result++;
      }
    }
  }
  return result;
}

static i32 count_touching(struct levelgen_ctx *ctx, i32 x, i32 y, u32 type) {
  i32 result = 0;
  #define CHECK(dx, dy)  do {                                   \
      if (ctx->board[(x + dx) + (y + dy) * BOARD_W] == type) {  \
        result++;                                               \
      }                                                         \
    } while (0)
  if (x >           0) CHECK(-1, 0);
  if (y >           0) CHECK(0, -1);
  if (y < BOARD_H - 1) CHECK(0, 1);
  if (x < BOARD_W - 1) CHECK(1, 0);
  return result;
  #undef CHECK
}

static i32 board_score(struct levelgen_ctx *ctx) {
  i32 score = 0;
  for (i32 y = 0, b = 0; y < BOARD_H; y++) {
    for (i32 x = 0; x < BOARD_W; x++, b++) {
      switch (ctx->board[b]) {
        case T_EYE: {
          if (x <= 1 || y <= 1 || x >= BOARD_W - 2 || y >= BOARD_H - 2) {
            score -= 10000;
          }
          u32 heals = 0;
          u32 walls = 0;
          for (i32 dy = -2; dy <= 2; dy++) {
            i32 by = dy + y;
            if (by < 0 || by >= BOARD_H) {
              score -= 10000;
              continue;
            }
            i32 dw = dy == -2 || dy == 2 ? 0 : dy == -1 || dy == 1 ? 1 : 2;
            by *= BOARD_W;
            for (i32 dx = -dw; dx <= dw; dx++) {
              if (dy == 0 && dx == 0) continue;
              i32 bx = dx + x;
              if (bx < 0 || bx >= BOARD_W) {
                score -= 10000;
                continue;
              }
              u8 type = ctx->board[bx + by];
              switch (type) {
                case T_LV5A:
                case T_LV5B:
                case T_LV8:
                case T_LV11:
                case T_LV13:
                case T_MINE:
                case T_CHEST_EYE:
                case T_CHEST_EXP:
                  score -= 2000;
                  break;
                case T_CHEST_HEAL:
                  heals++;
                  break;
                case T_WALL:
                  walls++;
                  break;
              }
            }
          }
          if (walls > 2) {
            score -= 2000 * (walls - 2);
          }
          if (heals == 1 && walls > 0) {
            score += 2000;
          }
          break;
        }
        case T_LV4:
          if (count_touching(ctx, x, y, T_LV4)) {
            score += 1000;
          }
          break;
        case T_LV6: {
          i32 chests = count_border(ctx, x, y, T_CHEST_HEAL);
          if (chests > 1) break;
          chests += count_border(ctx, x, y, T_CHEST_EYE);
          if (chests > 1) break;
          chests += count_border(ctx, x, y, T_CHEST_EXP);
          if (chests != 1) break;
          if (count_border_rad(ctx, x, y, 2, T_LV6)) break;
          score += 10000;
          break;
        }
        case T_LV7A:
          if (x < BOARD_CW && y < BOARD_CH) {
            score += 2500;
          }
          break;
        case T_LV7B:
          if (x >= BOARD_CW && y < BOARD_CH) {
            score += 2500;
          }
          break;
        case T_LV7C:
          if (x < BOARD_CW && y > BOARD_CH) {
            score += 2500;
          }
          break;
        case T_LV7D:
          if (x >= BOARD_CW && y > BOARD_CH) {
            score += 2500;
          }
          break;
        case T_WALL:
          if (count_touching(ctx, x, y, T_WALL) != 1) break;
          if (count_border(ctx, x, y, T_WALL) != 1) break;
          score += 2000;
          if (x == 1 || y == 1 || x == BOARD_W - 2 || y == BOARD_H - 2) {
            score += 2000;
          }
          break;
        case T_CHEST_HEAL:
        case T_CHEST_EYE:
        case T_CHEST_EXP:
          score -= 1000 * count_border_chest(ctx, x, y);
          break;
      }
    }
  }
  return score;
}

static void level_attach_unfixed(struct levelgen_ctx *ctx) {
  for (i32 k = 0; k < 4; k++) {
    for (i32 i = 0; i < ctx->unfixed_size; i++) {
      i32 best_score = board_score(ctx);
      u8 b = ctx->unfixed[i];
      for (i32 j = 0; j < BOARD_SIZE; j++) {
        u8 b2 = ctx->board_order[j];
        if (ctx->board[b2]) { // skip if something is already here (including itself)
          continue;
        }
        // see if this placement is better
        ctx->board[b2] =ctx->board[b];
        ctx->board[b] = 0;
        i32 score = board_score(ctx);
        if (score >= best_score) {
          // keep it!
          best_score = score;
          ctx->unfixed[i] = b = b2;
        } else {
          // revert it :-(
          ctx->board[b] =ctx->board[b2];
          ctx->board[b2] = 0;
        }
      }
    }
  }
  ctx->unfixed_size = 0;
}

static void level_add(struct levelgen_ctx *ctx, u32 type, u32 count) {
  for (i32 i = 0; i < BOARD_SIZE; i++) {
    u8 b = ctx->board_order[i];
    if (ctx->board[b] == T_EMPTY) {
      ctx->board[b] = type;
      ctx->unfixed[ctx->unfixed_size++] = b;
      count--;
      if (!count) {
        return;
      }
    }
  }
}

void levelgen_stage1(struct levelgen_ctx *ctx) {
  for (i32 i = 0; i < BOARD_SIZE; i++) {
    ctx->board_order[i] = i;
  }
  shuffle8(ctx, ctx->board_order, BOARD_SIZE);
restart:
  for (i32 i = 0; i < BOARD_SIZE; i++) {
    ctx->board[i] = 0;
  }
  ctx->unfixed_size = 0;

  // place easy items directly
  {
    // lv13 is in the center
    u32 bx = roll(ctx, 2) + BOARD_CW;
    u32 by = roll(ctx, 3) + BOARD_CH - 1;
    ctx->board[bx + by * BOARD_W] = T_LV13;

    // lv1b is always on the edge, surrounded by lv8's
    switch (roll(ctx, 4)) {
      case 0: bx = 0; by = roll(ctx, BOARD_H - 2) + 1; break;
      case 1: bx = BOARD_W - 1; by = roll(ctx, BOARD_H - 2) + 1; break;
      case 2: bx = roll(ctx, BOARD_W - 2) + 1; by = 0; break;
      case 3: bx = roll(ctx, BOARD_W - 2) + 1; by = BOARD_H - 1; break;
    }
    ctx->board[bx + by * BOARD_W] = T_LV1B;
    for (i32 dy = -1; dy <= 1; dy++) {
      i32 by2 = by + dy;
      if (by2 < 0 || by2 >= BOARD_H) continue;
      for (i32 dx = -1; dx <= 1; dx++) {
        i32 bx2 = bx + dx;
        if ((dy == 0 && dx == 0) || bx2 < 0 || bx2 >= BOARD_W) continue;
        ctx->board[bx2 + by2 * BOARD_W] = T_LV8;
      }
    }

    // lv10 is always in a corner
    for (;;) {
      switch (roll(ctx, 4)) {
        case 0: bx = 0; by = 0; break;
        case 1: bx = BOARD_W - 1; by = 0; break;
        case 2: bx = 0; by = BOARD_H - 1; break;
        case 3: bx = BOARD_W - 1; by = BOARD_H - 1; break;
      }
      bx += by * BOARD_W;
      if (ctx->board[bx]) continue;
      ctx->board[bx] = T_LV10;
      break;
    }

    // two lv9's are always mirrored
    for (;;) {
      u32 bx = roll(ctx, BOARD_CW - 2) + 1;
      u32 bx1 = BOARD_CW - 1 - bx;
      u32 bx2 = BOARD_CW + bx;
      u32 by1 = roll(ctx, BOARD_H);
      u32 by2 = roll(ctx, 4) ? roll(ctx, BOARD_H) : by1;
      u32 k1 = bx1 + by1 * BOARD_W;
      u32 k2 = bx2 + by2 * BOARD_W;
      if (ctx->board[k1] || ctx->board[k2]) continue;
      ctx->board[k1] = T_LV9;
      ctx->board[k2] = T_LV9;
      break;
    }
  }

  level_add(ctx, T_LV5A, 1); // rat king
  level_add(ctx, T_WALL, 6);
  level_add(ctx, T_LV6, 5);
  level_add(ctx, T_LV7A, 1);
  level_add(ctx, T_LV7B, 1);
  level_add(ctx, T_LV7C, 1);
  level_add(ctx, T_LV7D, 1);
  level_add(ctx, T_LV4, 8);
  level_add(ctx, T_LV5B, 2); // gazer
  level_add(ctx, T_MINE, 9);
  level_add(ctx, T_CHEST_HEAL, 7);
  level_add(ctx, T_EYE, 1);
  level_add(ctx, T_CHEST_EXP, 3);
  level_attach_unfixed(ctx);

  // verify that no location has more than 4 mines next to it
  for (i32 y = 0; y < BOARD_H; y++) {
    for (i32 x = 0; x < BOARD_W; x++) {
      if (count_border(ctx, x, y, T_MINE) > 4) {
        goto restart;
      }
    }
  }
}

void levelgen_stage2(struct levelgen_ctx *ctx) {
  for (i32 i = 0; i < BOARD_SIZE; i++) {
    ctx->board_order[i] = i;
  }
  shuffle8(ctx, ctx->board_order, BOARD_SIZE);
  ctx->unfixed_size = 0;

  level_add(ctx, T_LV1A, 12); // rat
  level_add(ctx, T_LV2, 11);
  level_add(ctx, T_LV3, 9);
  level_add(ctx, T_LV5C, 8); // slime
  level_add(ctx, T_LV11, 1);
  // TODO: 1 gnome
  level_add(ctx, T_CHEST_EYE, 1);
  level_attach_unfixed(ctx);
}

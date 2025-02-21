//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#include "game.h"

static i32 count_border(struct levelgen_st *ctx, i32 x, i32 y, u32 type) {
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

static i32 count_border_rad(struct levelgen_st *ctx, i32 x, i32 y, i32 rad, u32 type) {
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

static i32 count_border_chest(struct levelgen_st *ctx, i32 x, i32 y) {
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
      if (IS_CHEST(type) || type == T_ITEM_EYE) {
        result++;
      }
    }
  }
  return result;
}

static i32 count_touching(struct levelgen_st *ctx, i32 x, i32 y, u32 type) {
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

static i32 board_score(struct levelgen_st *ctx) {
  i32 score = 0;
  for (i32 y = 0, b = 0; y < BOARD_H; y++) {
    for (i32 x = 0; x < BOARD_W; x++, b++) {
      switch (GET_TYPE(ctx->board[b])) {
        case T_ITEM_EYE:
        case T_CHEST_EYE2: {
          u32 boundary = GET_TYPE(ctx->board[b]) == T_ITEM_EYE ? 1 : 0;
          u32 heals = 0;
          u32 walls = 0;
          for (i32 dy = -2; dy <= 2; dy++) {
            i32 by = dy + y;
            if (by < boundary || by >= BOARD_H - boundary) {
              score -= 10000;
              continue;
            }
            i32 dw = dy == -2 || dy == 2 ? 0 : dy == -1 || dy == 1 ? 1 : 2;
            by *= BOARD_W;
            for (i32 dx = -dw; dx <= dw; dx++) {
              if (dy == 0 && dx == 0) continue;
              i32 bx = dx + x;
              if (bx < boundary || bx >= BOARD_W - boundary) {
                score -= 10000;
                continue;
              }
              u8 type = ctx->board[bx + by];
              switch (type) {
                case T_LV5A:
                case T_LV5B:
                case T_LV8:
                case T_LV11:
                case T_MINE:
                case T_CHEST_EXP:
                  score -= 2000;
                  break;
                case T_CHEST_HEAL:
                  heals++;
                  break;
                case T_LV13:
                case T_CHEST_EYE2:
                case T_ITEM_EYE:
                  score -= 10000;
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
          chests += count_border(ctx, x, y, T_CHEST_EYE2);
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
        case T_CHEST_EXP:
          score -= 1000 * count_border_chest(ctx, x, y);
          break;
      }
    }
  }
  return score;
}

static void level_attach_unfixed(struct levelgen_st *ctx) {
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

static void level_add(struct levelgen_st *ctx, u32 type, u32 count) {
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

void levelgen_stage1(struct levelgen_st *ctx) {
  for (i32 i = 0; i < BOARD_SIZE; i++) {
    ctx->board_order[i] = i;
  }
  shuffle8(&ctx->rnd, ctx->board_order, BOARD_SIZE);
restart:
  for (i32 i = 0; i < BOARD_SIZE; i++) {
    ctx->board[i] = 0;
  }
  ctx->unfixed_size = 0;

  // place easy items directly
  {
    // lv13 is in the center
    u32 bx = roll(&ctx->rnd, 2) + BOARD_CW;
    u32 by = roll(&ctx->rnd, 3) + BOARD_CH - 1;
    ctx->board[bx + by * BOARD_W] = T_LV13;

    // lv1b is always on the edge, surrounded by lv8's
    switch (roll(&ctx->rnd, 4)) {
      case 0: bx = 0; by = roll(&ctx->rnd, BOARD_H - 2) + 1; break;
      case 1: bx = BOARD_W - 1; by = roll(&ctx->rnd, BOARD_H - 2) + 1; break;
      case 2: bx = roll(&ctx->rnd, BOARD_W - 2) + 1; by = 0; break;
      case 3: bx = roll(&ctx->rnd, BOARD_W - 2) + 1; by = BOARD_H - 1; break;
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
      switch (roll(&ctx->rnd, 4)) {
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
      u32 bx = roll(&ctx->rnd, BOARD_CW - 2) + 1;
      u32 bx1 = BOARD_CW - 1 - bx;
      u32 bx2 = BOARD_CW + bx;
      u32 by = roll(&ctx->rnd, BOARD_H);
      u32 k1 = bx1 + by * BOARD_W;
      u32 k2 = bx2 + by * BOARD_W;
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
  level_add(ctx, T_CHEST_EYE2, 1);
  level_add(ctx, T_ITEM_EYE | 0x80, 1); // collectable at the start
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

void levelgen_stage2(struct levelgen_st *ctx) {
  for (i32 i = 0; i < BOARD_SIZE; i++) {
    ctx->board_order[i] = i;
  }
  shuffle8(&ctx->rnd, ctx->board_order, BOARD_SIZE);
  ctx->unfixed_size = 0;

  level_add(ctx, T_LV1A, 12); // rat
  level_add(ctx, T_LV2, 11);
  level_add(ctx, T_LV3, 9);
  level_add(ctx, T_LV5C, 8); // slime
  level_add(ctx, T_LV11, 1);
  // TODO: 1 gnome
  level_attach_unfixed(ctx);
}

void levelgen_onlymines(struct levelgen_st *ctx, u32 difficulty) {
  // beginner: 10/81
  // intermediate: 40/256
  // expert: 99/480
  u8 minecount_table[] = { 16, 18, 20, 22, 26 };
  u8 minecount = minecount_table[difficulty];
  for (i32 i = 0; i < minecount; i++) {
    for (;;) {
      i32 x = roll(&ctx->rnd, BOARD_W);
      i32 y = roll(&ctx->rnd, BOARD_H);
      if (x == BOARD_CW && y == BOARD_CH) continue; // never place directly under starting position
      i32 k = x + y * BOARD_W;
      if (ctx->board[k] == 0) {
        ctx->board[k] = T_MINE;
        break;
      }
    }
  }
}

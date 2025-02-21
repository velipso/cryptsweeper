//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#include "game.h"
#include <stddef.h>

enum tile_info_action {
  TI_ICON,
  TI_THREAT,
  TI_COLLECT,
  TI_ATTACK
};

static i32 tile_info(
  struct game_st *game,
  game_handler_f handler,
  u32 type,
  enum tile_info_action action
);

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

void game_new(struct game_st *game, struct levelgen_st *ctx) {
}

void game_hover(struct game_st *game, game_handler_f f_handler, i32 x, i32 y) {
  game->selx = x;
  game->sely = y;
}

static void check_onlymines(struct game_st *game, game_handler_f handler) {
  u8 board[BOARD_SIZE];
  for (i32 i = 0; i < BOARD_SIZE; i++) {
    board[i] = 0;
  }
  i32 selx = game->selx;
  i32 sely = game->sely;
  board[game->selx + game->sely * BOARD_W] = 2;
  for (;;) {
    for (i32 y = 0, k = 0; y < BOARD_H; y++) {
      for (i32 x = 0; x < BOARD_W; x++, k++) {
        if (board[k] == 2) {
          board[k] = 3;
          if (game->board[k] == T_EMPTY) {
            if (count_threat(game->board, x, y) == 0) {
              for (i32 dy = -1; dy <= 1; dy++) {
                i32 by = dy + y;
                if (by < 0 || by >= BOARD_H) continue;
                for (i32 dx = -1; dx <= 1; dx++) {
                  if (dy == 0 && dx == 0) continue;
                  i32 bx = dx + x;
                  if (bx < 0 || bx >= BOARD_W) continue;
                  i32 k2 = bx + by * BOARD_W;
                  if (board[k2] == 0) board[k2] = 1;
                }
              }
            }
          }
          game->selx = x;
          game->sely = y;
          SET_STATUS(game->board[k], S_PRESSED);
          tile_info(game, handler, game->board[k], TI_ATTACK);
          handler(game, EV_TILE_UPDATE, x, y);
        }
      }
    }
    bool found = false;
    for (i32 i = 0; i < BOARD_SIZE; i++) {
      if (board[i] == 1) {
        found = true;
        board[i] = 2;
      }
    }
    if (!found) break;
    handler(game, EV_WAIT, 1, 0);
  }
  game->selx = selx;
  game->sely = sely;

  // check if we won
  if (!game->win) {
    bool won = true;
    for (i32 y = 0, k = 0; y < BOARD_H; y++) {
      for (i32 x = 0; x < BOARD_W; x++, k++) {
        if (game->board[k] == T_EMPTY) {
          won = false;
        }
      }
    }
    if (won) {
      game->win = 2;
      handler(game, EV_YOU_WIN, 0, 0);
    }
  }
}

bool game_click(struct game_st *game, game_handler_f handler) {
  bool result = true;
  u32 k = game->selx + game->sely * BOARD_W;
  if (game->difficulty & D_ONLYMINES) {
    if (GET_STATUS(game->board[k]) != S_PRESSED) {
      if (game->notes[k] == -2) {
        // can't click on flagged mines
        return false;
      } else {
        check_onlymines(game, handler);
      }
    }
  } else {
    if (GET_STATUS(game->board[k]) == S_PRESSED) {
      if (GET_TYPE(game->board[k]) == T_EMPTY) {
        return false;
      }
      result = tile_info(game, handler, game->board[k], TI_COLLECT) == 0;
    } else {
      SET_STATUS(game->board[k], S_PRESSED);
      result = tile_info(game, handler, game->board[k], TI_ATTACK) == 0;
    }
    for (i32 dy = -1; dy <= 1; dy++) {
      for (i32 dx = -1; dx <= 1; dx++) {
        handler(game, EV_TILE_UPDATE, dx + game->selx, dy + game->sely);
      }
    }
  }
  return result;
}

bool game_levelup(struct game_st *game, game_handler_f handler) {
  if (!(game->difficulty & D_ONLYMINES) && game->exp >= max_exp(game)) {
    // level up!
    game->exp -= max_exp(game);
    game->level++;
    game->hp = max_hp(game);
    handler(game, EV_HP_UPDATE, game->hp, max_hp(game));
    handler(game, EV_EXP_UPDATE, game->exp, max_exp(game));
    return true;
  }
  return false;
}

void game_note(struct game_st *game, game_handler_f handler, i8 note) {
  game->notes[game->selx + game->sely * BOARD_W] = note;
  handler(game, EV_TILE_UPDATE, game->selx, game->sely);
}

i32 game_tileicon(u32 type) {
  return tile_info(NULL, NULL, type, TI_ICON);
}

i32 max_hp(struct game_st *game) {
  static const i32 table[] = {5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12};
  if (game->level < 16) {
    return table[game->level];
  }
  return 13;
}

i32 max_exp(struct game_st *game) {
  static const i32 table[] = {4, 5, 7, 9, 9, 10, 12, 12, 12, 15, 18, 21, 21, 25};
  if (game->level < 14) {
    return table[game->level];
  }
  return 25;
}

static i32 threat_at(const u8 *board, i32 x, i32 y) {
  if (x < 0 || x >= BOARD_W || y < 0 || y >= BOARD_H) return 0;
  i32 res = tile_info(NULL, NULL, board[x + y * BOARD_W], TI_THREAT);
  if (res < 0) {
    return 0;
  }
  return res;
}

i32 count_threat(const u8 *board, i32 x, i32 y) {
  i32 result = -threat_at(board, x, y);
  for (i32 dy = -1; dy <= 1; dy++) {
    i32 by = dy + y;
    for (i32 dx = -1; dx <= 1; dx++) {
      i32 bx = dx + x;
      result += threat_at(board, bx, by);
    }
  }
  return result;
}

static i32 you_died(struct game_st *game, game_handler_f handler) {
  SET_STATUS(game->board[game->selx + game->sely * BOARD_W], S_KILLED);
  handler(game, EV_TILE_UPDATE, game->selx, game->sely);
  game->win = 1;
  if (!(game->difficulty & D_ONLYMINES)) {
    // remove "level up" animation if it's there
    handler(game, EV_EXP_UPDATE, game->exp, max_exp(game));
  }
  handler(game, EV_YOU_LOSE, game->selx, game->sely);
  for (i32 y = 0, k = 0; y < BOARD_H; y++) {
    for (i32 x = 0; x < BOARD_W; x++, k++) {
      if (GET_STATUS(game->board[k]) == S_HIDDEN) {
        SET_STATUS(game->board[k], S_VISIBLE);
      }
      handler(game, EV_TILE_UPDATE, x, y);
    }
    handler(game, EV_WAIT, 2, 0);
  }
  return 0;
}

static void award_exp(struct game_st *game, game_handler_f handler, i32 amt) {
  game->exp += amt;
  handler(game, EV_EXP_UPDATE, game->exp, max_exp(game));
}

static i32 replace_type(struct game_st *game, game_handler_f handler, u32 t) {
  SET_TYPE(game->board[game->selx + game->sely * BOARD_W], t);
  return 0;
}

static i32 collect_monster_item(struct game_st *game, game_handler_f handler, i32 new_item) {
  u32 k = game->selx + game->sely * BOARD_W;
  i32 exp = tile_info(game, handler, game->board[k], TI_THREAT);
  award_exp(game, handler, exp);
  return replace_type(game, handler, new_item);
}

static i32 collect_monster(struct game_st *game, game_handler_f handler) {
  return collect_monster_item(game, handler, T_EMPTY);
}

static i32 attack_monster(struct game_st *game, game_handler_f handler) {
  u32 k = game->selx + game->sely * BOARD_W;
  i32 exp = tile_info(game, handler, game->board[k], TI_THREAT);
  if (exp > game->hp) {
    game->hp = 0;
    handler(game, EV_HP_UPDATE, game->hp, max_hp(game));
    return you_died(game, handler);
  }
  game->hp -= exp;
  handler(game, EV_HP_UPDATE, game->hp, max_hp(game));
  return 0;
}

static void reveal(struct game_st *game, game_handler_f handler, i32 x, i32 y) {
  if (x < 0 || x >= BOARD_W || y < 0 || y >= BOARD_H) return;
  i32 k = x + y * BOARD_W;
  if (GET_STATUS(game->board[k]) == S_HIDDEN) {
    if (
      GET_TYPE(game->board[k]) == T_EMPTY ||
      IS_ITEM(game->board[k]) ||
      IS_CHEST(game->board[k])
    ) {
      SET_STATUS(game->board[k], S_PRESSED);
    } else {
      SET_STATUS(game->board[k], S_VISIBLE);
    }
    handler(game, EV_TILE_UPDATE, x, y);
  }
}

static i32 tile_info(
  struct game_st *game,
  game_handler_f handler,
  u32 type,
  enum tile_info_action action
) {
  #define TILE(px, py)  (px >> 3) + ((py >> 3) << 5)
  switch (GET_TYPE(type)) {
    case T_EMPTY:
      switch (action) {
        case TI_ICON: return 0;
        case TI_THREAT: return 0;
        case TI_COLLECT: return 0;
        case TI_ATTACK: return 0;
      }
      break;
    case T_LV1A:
      switch (action) {
        case TI_ICON: return TILE(160, 0);
        case TI_THREAT: return 1;
        case TI_COLLECT: return collect_monster(game, handler);
        case TI_ATTACK: return attack_monster(game, handler);
      }
      break;
    case T_LV1B:
      switch (action) {
        case TI_ICON: return TILE(160, 16);
        case TI_THREAT: return 1;
        case TI_COLLECT: return collect_monster_item(game, handler, T_ITEM_SHOW5);
        case TI_ATTACK: return attack_monster(game, handler);
      }
      break;
    case T_LV2:
      switch (action) {
        case TI_ICON: return TILE(160, 32);
        case TI_THREAT: return 2;
        case TI_COLLECT: return collect_monster(game, handler);
        case TI_ATTACK: return attack_monster(game, handler);
      }
      break;
    case T_LV3:
      switch (action) {
        case TI_ICON: return TILE(160, 48);
        case TI_THREAT: return 3;
        case TI_COLLECT: return collect_monster(game, handler);
        case TI_ATTACK: return attack_monster(game, handler);
      }
      break;
    case T_LV4:
      switch (action) {
        case TI_ICON: return TILE(160, 64);
        case TI_THREAT: return 4;
        case TI_COLLECT: return collect_monster(game, handler);
        case TI_ATTACK: return attack_monster(game, handler);
      }
      break;
    case T_LV5A:
      switch (action) {
        case TI_ICON: return TILE(160, 80);
        case TI_THREAT: return 5;
        case TI_COLLECT: return collect_monster_item(game, handler, T_ITEM_SHOW1);
        case TI_ATTACK: return attack_monster(game, handler);
      }
      break;
    case T_LV5B:
      switch (action) {
        case TI_ICON: return TILE(160, 96);
        case TI_THREAT: return 5;
        case TI_COLLECT: return collect_monster(game, handler);
        case TI_ATTACK: return attack_monster(game, handler);
      }
      break;
    case T_LV5C:
      switch (action) {
        case TI_ICON: return TILE(160, 112);
        case TI_THREAT: return 5;
        case TI_COLLECT: return collect_monster(game, handler);
        case TI_ATTACK: return attack_monster(game, handler);
      }
      break;
    case T_LV6:
      switch (action) {
        case TI_ICON: return TILE(208, 0);
        case TI_THREAT: return 6;
        case TI_COLLECT: return collect_monster(game, handler);
        case TI_ATTACK: return attack_monster(game, handler);
      }
      break;
    case T_LV7A:
    case T_LV7B:
    case T_LV7C:
    case T_LV7D:
      switch (action) {
        case TI_ICON: return TILE(208, 16);
        case TI_THREAT: return 7;
        case TI_COLLECT: return collect_monster(game, handler);
        case TI_ATTACK: return attack_monster(game, handler);
      }
      break;
    case T_LV8:
      switch (action) {
        case TI_ICON: return TILE(208, 32);
        case TI_THREAT: return 8;
        case TI_COLLECT: return collect_monster(game, handler);
        case TI_ATTACK: return attack_monster(game, handler);
      }
      break;
    case T_LV9:
      switch (action) {
        case TI_ICON: return TILE(208, 48);
        case TI_THREAT: return 9;
        case TI_COLLECT: return collect_monster_item(game, handler, T_ITEM_HEAL);
        case TI_ATTACK: return attack_monster(game, handler);
      }
      break;
    case T_LV10:
      switch (action) {
        case TI_ICON: return TILE(208, 64);
        case TI_THREAT: return 10;
        case TI_COLLECT: return collect_monster(game, handler);
        case TI_ATTACK: return attack_monster(game, handler);
      }
      break;
    case T_LV11:
      switch (action) {
        case TI_ICON: return TILE(208, 80);
        case TI_THREAT: return 11;
        case TI_COLLECT: return collect_monster(game, handler);
        case TI_ATTACK: return attack_monster(game, handler);
      }
      break;
    case T_LV13:
      switch (action) {
        case TI_ICON: return TILE(208, 96);
        case TI_THREAT: return 13;
        case TI_COLLECT: return collect_monster_item(game, handler, T_ITEM_EXIT);
        case TI_ATTACK: return attack_monster(game, handler);
      }
      break;
    case T_MINE:
      switch (action) {
        case TI_ICON: return TILE(64, 0);
        case TI_THREAT: return 0x100;
        case TI_COLLECT: return you_died(game, handler);
        case TI_ATTACK: return you_died(game, handler);
      }
      break;
    case T_WALL:
      switch (action) {
        case TI_ICON: return TILE(64, 32);
        case TI_THREAT: return 0;
        case TI_COLLECT: {
          if (game->hp <= 0) {
            return -1;
          }
          i32 item = game->hp >= 5 ? T_ITEM_EXP5 : game->hp >= 3 ? T_ITEM_EXP3 : T_ITEM_EXP1;
          game->hp = 0;
          handler(game, EV_HP_UPDATE, game->hp, max_hp(game));
          return replace_type(game, handler, item);
        }
        case TI_ATTACK: return 0;
      }
      break;
    case T_CHEST_HEAL:
      switch (action) {
        case TI_ICON: return TILE(64, 16);
        case TI_THREAT: return 0;
        case TI_COLLECT: return replace_type(game, handler, T_ITEM_HEAL);
        case TI_ATTACK: return 0;
      }
      break;
    case T_CHEST_EYE2:
      switch (action) {
        case TI_ICON: return TILE(64, 16);
        case TI_THREAT: return 0;
        case TI_COLLECT: return replace_type(game, handler, T_ITEM_EYE2);
        case TI_ATTACK: return 0;
      }
      break;
    case T_CHEST_EXP:
      switch (action) {
        case TI_ICON: return TILE(64, 16);
        case TI_THREAT: return 0;
        case TI_COLLECT: return replace_type(game, handler, T_ITEM_EXP5);
        case TI_ATTACK: return 0;
      }
      break;
    case T_ITEM_HEAL:
      switch (action) {
        case TI_ICON: return TILE(0, 112);
        case TI_THREAT: return 0;
        case TI_COLLECT:
          game->hp = max_hp(game);
          handler(game, EV_HP_UPDATE, game->hp, max_hp(game));
          return replace_type(game, handler, T_EMPTY);
        case TI_ATTACK: return 0;
      }
      break;
    case T_ITEM_EYE:
      switch (action) {
        case TI_ICON: return TILE(16, 112);
        case TI_THREAT: return 0;
        case TI_COLLECT:
          for (i32 dy = -2; dy <= 2; dy++) {
            i32 w = dy == 0 ? 2 : dy == 1 || dy == -1 ? 1 : 0;
            for (i32 dx = -w; dx <= w; dx++) {
              reveal(game, handler, dx + game->selx, dy + game->sely);
            }
          }
          return replace_type(game, handler, T_EMPTY);
        case TI_ATTACK: return 0;
      }
      break;
    case T_ITEM_EYE2:
      switch (action) {
        case TI_ICON: return TILE(16, 112);
        case TI_THREAT: return 0;
        case TI_COLLECT: {
          // we want to find the best spot to reveal, so we will score each location
          i32 best_score = 0;
          i32 best_x = 0;
          i32 best_y = 0;
          i32 same_score = 0;
          for (i32 y = 0; y < BOARD_H - 3; y++) {
            for (i32 x = 0; x < BOARD_W - 3; x++) {
              // calculate score of revealing this location
              i32 score = 0;
              i32 minecount = 0;
              i32 wallcount = 0;
              for (i32 dy = 0; dy < 3; dy++) {
                for (i32 dx = 0; dx < 3; dx++) {
                  i32 k = (x + dx) + (y + dy) * BOARD_W;
                  u8 t = game->board[k];
                  if (GET_STATUS(t) == S_HIDDEN) {
                    if (threat_at(game->board, x + dx, y + dy) < 8) {
                      score++;
                    }
                    score += 3;
                    switch (GET_TYPE(t)) {
                      case T_LV8:
                        score -= 10;
                        break;
                      case T_MINE:
                        minecount++;
                        break;
                      case T_WALL:
                        wallcount++;
                        break;
                    }
                  }
                }
              }
              if (minecount == 1) score += 12;
              if (wallcount == 1) score += 9;
              else if (wallcount > 1) score -= 9;
              if (x == 0 && y == 0) {
                best_score = score;
              } else {
                if (score > best_score) {
                  best_score = score;
                  best_x = x;
                  best_y = y;
                  same_score = 0;
                } else if (score == best_score) {
                  same_score++;
                  if (rnd_pick(&game->rnd, same_score)) {
                    best_x = x;
                    best_y = y;
                  }
                }
              }
            }
          }
          // reveal at best_x/y
          for (i32 dy = 0; dy < 3; dy++) {
            for (i32 dx = 0; dx < 3; dx++) {
              reveal(game, handler, best_x + dx, best_y + dy);
            }
          }
          return replace_type(game, handler, T_EMPTY);
        }
        case TI_ATTACK: return 0;
      }
      break;
    case T_ITEM_SHOW1:
      switch (action) {
        case TI_ICON: return TILE(32, 112);
        case TI_THREAT: return 0;
        case TI_COLLECT:
          for (i32 y = 0, k = 0; y < BOARD_H; y++) {
            for (i32 x = 0; x < BOARD_W; x++, k++) {
              if (GET_TYPE(game->board[k]) == T_LV1A) {
                reveal(game, handler, x, y);
              }
            }
          }
          return replace_type(game, handler, T_EMPTY);
        case TI_ATTACK: return 0;
      }
      break;
    case T_ITEM_SHOW5:
      switch (action) {
        case TI_ICON: return TILE(48, 112);
        case TI_THREAT: return 0;
        case TI_COLLECT:
        for (i32 y = 0, k = 0; y < BOARD_H; y++) {
          for (i32 x = 0; x < BOARD_W; x++, k++) {
            i32 t = GET_TYPE(game->board[k]);
            if (t == T_LV5C || t == T_LV8) {
              reveal(game, handler, x, y);
            }
          }
        }
        return replace_type(game, handler, T_EMPTY);
        case TI_ATTACK: return 0;
      }
      break;
    case T_ITEM_EXP1:
      switch (action) {
        case TI_ICON: return TILE(64, 112);
        case TI_THREAT: return 0;
        case TI_COLLECT:
          award_exp(game, handler, 1);
          return replace_type(game, handler, T_EMPTY);
        case TI_ATTACK: return 0;
      }
      break;
    case T_ITEM_EXP3:
      switch (action) {
        case TI_ICON: return TILE(80, 112);
        case TI_THREAT: return 0;
        case TI_COLLECT:
          award_exp(game, handler, 3);
          return replace_type(game, handler, T_EMPTY);
        case TI_ATTACK: return 0;
      }
      break;
    case T_ITEM_EXP5:
      switch (action) {
        case TI_ICON: return TILE(96, 112);
        case TI_THREAT: return 0;
        case TI_COLLECT:
          award_exp(game, handler, 5);
          return replace_type(game, handler, T_EMPTY);
        case TI_ATTACK: return 0;
      }
      break;
    case T_ITEM_EXIT:
      switch (action) {
        case TI_ICON: return TILE(112, 112);
        case TI_THREAT: return 0;
        case TI_COLLECT:
          game->win = 2;
          handler(game, EV_YOU_WIN, 0, 0);
          return 0;
        case TI_ATTACK: return 0;
      }
      break;
  }
  #undef TILE
  return -1;
}

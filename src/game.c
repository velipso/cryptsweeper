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

void game_new(struct game_st *game, i32 difficulty, u32 seed) {
  rnd_seed(&game->rnd, seed);
  game->level = 0;
  game->hp = 0;
  game->exp = 0;
  game->difficulty = difficulty;
  game->win = 0;
  game->hp = max_hp(game);
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
      if (game->notes[k] == -2) {
        // can't click on flagged mines
        return false;
      }
      SET_STATUS(game->board[k], S_PRESSED);
      result = tile_info(game, handler, game->board[k], TI_ATTACK) == 0;
    }
    for (i32 dy = -2; dy <= 2; dy++) {
      for (i32 dx = -2; dx <= 2; dx++) {
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
  i32 result = 0;
  for (i32 dy = -1; dy <= 1; dy++) {
    i32 by = dy + y;
    for (i32 dx = -1; dx <= 1; dx++) {
      if (dy == 0 && dx == 0) continue;
      i32 bx = dx + x;
      result += threat_at(board, bx, by);
    }
  }

  // check if we're in range of a lv5c
  bool hidden = false;
  i32 w = 0;
  for (i32 dy = -2; dy <= 2 && !hidden; dy++) {
    i32 by = dy + y;
    if (by < 0 || by >= BOARD_H) goto next_dy;
    for (i32 dx = -w; dx <= w && !hidden; dx++) {
      i32 bx = dx + x;
      if (bx < 0 || bx >= BOARD_W) continue;
      if (GET_TYPEXY(board, bx, by) == T_LV5C) {
        hidden = true;
      }
    }
next_dy:
    if (dy < 0) w++;
    else w--;
  }
  if (hidden) {
    // mask threat
    result |= 0xff;
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
      GET_TYPE(game->board[k]) == T_WALL ||
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
        case TI_ICON: return TILE(160, 96);
        case TI_THREAT: return 5;
        case TI_COLLECT: return collect_monster(game, handler);
        case TI_ATTACK: return attack_monster(game, handler);
      }
      break;
    case T_LV5B:
      switch (action) {
        case TI_ICON: return TILE(160, 80);
        case TI_THREAT: return 5;
        case TI_COLLECT: return collect_monster_item(game, handler, T_ITEM_SHOW1);
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
    case T_LV7:
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
        case TI_ICON: return TILE(128, 112);
        case TI_THREAT: return 0;
        case TI_COLLECT: {
          if (game->hp <= 0) {
            return -1;
          }
          // spend extra health for exp
          i32 item = T_EMPTY;
          if (game->hp >= 6) {
            game->hp -= 6;
            item = T_ITEM_EXP5;
          } else if (game->hp >= 4) {
            game->hp -= 4;
            item = T_ITEM_EXP3;
          } else if (game->hp >= 2) {
            game->hp -= 2;
            item = T_ITEM_EXP1;
          } else {
            game->hp = 0;
          }
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
            if (t == T_LV5A || t == T_LV8) {
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

static i32 known_threat_at(struct game_st *game, i32 x, i32 y) {
  i32 k = x + y * BOARD_W;
  i32 s = GET_STATUS(game->board[k]);
  if (s == S_HIDDEN) {
    if (game->notes[k] == -2) { // noted mine
      return 0x100;
    } else if (game->notes[k] > 0) { // noted threat
      return game->notes[k];
    }
    return -1; // unknown
  }
  // visible or pressed, so query type directly
  return threat_at(game->board, x, y);
}

static i32 remaining_count_threat(struct game_st *game, i32 x, i32 y) {
  i32 ct = count_threat(game->board, x, y);
  for (i32 dy = -1; dy <= 1; dy++) {
    i32 by = y + dy;
    if (by < 0 || by >= BOARD_H) continue;
    for (i32 dx = -1; dx <= 1; dx++) {
      if (dy == 0 && dx == 0) continue;
      i32 bx = x + dx;
      if (bx < 0 || bx >= BOARD_W) continue;
      i32 kt = known_threat_at(game, bx, by);
      if (kt >= 0x100) {
        ct -= 0x100;
      } else if (kt > 0 && kt < 0xff) {
        if ((ct & 0xff) != 0xff) {
          ct -= kt;
        }
      }
    }
  }
  return ct;
}

static bool tile_could_threat(struct game_st *game, i32 x, i32 y, i32 threat) {
  // hypothesis: the tile at (x,y) has threat `threat`
  for (i32 dy = -1; dy <= 1; dy++) {
    i32 by = y + dy;
    if (by < 0 || by >= BOARD_H) continue;
    for (i32 dx = -1; dx <= 1; dx++) {
      if (dy == 0 && dx == 0) continue;
      i32 bx = x + dx;
      if (bx < 0 || bx >= BOARD_W) continue;
      i32 bk = bx + by * BOARD_W;
      i32 s = GET_STATUS(game->board[bk]);
      i32 t = GET_TYPE(game->board[bk]);
      if (s == S_PRESSED && t == T_EMPTY) {
        i32 ct = remaining_count_threat(game, bx, by);
        if (threat > ct) {
          // an adjacent cell is telling us that the real threat can't exceed ct
          // so we've proven our hypothesis false
          return false;
        }
      }
    }
  }
  return true;
}

u32 game_hint(struct game_st *game, game_handler_f handler) {
  #define H_CLICK(x, y)    (0x00000000 | (x) | ((y) << 8))
  #define H_NOTE(x, y, n)  (0x00010000 | (x) | ((y) << 8) | (((u8)n) << 24))
  #define H_LEVELUP()      0x00020000
  #define H_GIVEUP()       0x00030000

  if (game->hp == 0 && game->exp >= max_exp(game)) {
    // level up aggressively to restore health asap
    return H_LEVELUP();
  }
  i32 heal_x = -1, heal_y = -1;
  for (i32 y = 0, k = 0; y < BOARD_H; y++) {
    for (i32 x = 0; x < BOARD_W; x++, k++) {
      i32 s = GET_STATUS(game->board[k]);
      i32 t = GET_TYPE(game->board[k]);
      if ((s == S_VISIBLE || s == S_PRESSED) && t == T_ITEM_HEAL) {
        heal_x = x;
        heal_y = y;
      }
      if (
        // always collect items (except healing), exp, and chests ASAP
        ((s == S_VISIBLE || s == S_PRESSED) && (
          t == T_ITEM_EYE ||
          t == T_ITEM_EYE2 ||
          t == T_ITEM_EXP1 ||
          t == T_ITEM_EXP3 ||
          t == T_ITEM_EXP5 ||
          t == T_ITEM_SHOW1 ||
          t == T_ITEM_SHOW5 ||
          t == T_ITEM_EXIT ||
          IS_CHEST(t)
        )) ||
        (s == S_PRESSED && IS_MONSTER(t))
      ) {
        return H_CLICK(x, y);
      } else if (s == S_PRESSED && t == T_EMPTY) {
        // attempt to note adjacent cells based on this threat
        i32 threat = remaining_count_threat(game, x, y);
        i32 ux = -1, uy = -1, uc = 0;
        for (i32 dy = -1; dy <= 1; dy++) {
          i32 by = y + dy;
          if (by < 0 || by >= BOARD_H) continue;
          for (i32 dx = -1; dx <= 1; dx++) {
            if (dy == 0 && dx == 0) continue;
            i32 bx = x + dx;
            if (bx < 0 || bx >= BOARD_W) continue;
            if (known_threat_at(game, bx, by) < 0) {
              // unknown
              uc++;
              ux = bx;
              uy = by;
            }
          }
        }
        if (uc > 0) {
          if (threat == 0) {
            // no threat here, so click all cells around it
            return H_CLICK(ux, uy);
          }
          if (uc == 1) {
            // a single unknown, and we know the threat, so note it
            if (threat > 0 && threat < 0xff) {
              return H_NOTE(ux, uy, threat);
            } else if (threat == 0x100 || threat == 0x1ff) {
              return H_NOTE(ux, uy, -2); // mine
            }
          }
        }
      }
    }
  }

  // find a useful cell to challenge
  i32 best_score = 0;
  i32 same_score = 0;
  i32 best_x = -1;
  i32 best_y = -1;
  for (i32 y = 0, k = 0; y < BOARD_H; y++) {
    for (i32 x = 0; x < BOARD_W; x++, k++) {
      i32 score = 0;

      // get the threat (or worst case scenario)
      i32 threat = known_threat_at(game, x, y);
      if (threat < 0) { // unknown threat, calculate worst case
        score += 100; // favor cells that have unknown threats
        threat = 0x100;
        if (!tile_could_threat(game, x, y, 0x100)) {
          for (i32 ct = 11; ct >= 1; ct--) {
            if (tile_could_threat(game, x, y, ct)) {
              threat = ct;
              break;
            }
          }
        }
      }

      if (threat == 0) {
        if (GET_STATUS(game->board[k]) == S_HIDDEN) {
          // click hidden non-threats immediately
          return H_CLICK(x, y);
        }
      } else if (threat <= game->hp) {
        // we *could* kill this monster... should we?
        // see how many cells we would get information about
        i32 hidden = 0;
        for (i32 dy = -1; dy <= 1; dy++) {
          i32 by = dy + y;
          if (by < 0 || by >= BOARD_H) continue;
          for (i32 dx = -1; dx <= 1; dx++) {
            i32 bx = dx + x;
            if (bx < 0 || bx >= BOARD_W) continue;
            if (GET_STATUSXY(game->board, bx, by) == S_HIDDEN) {
              hidden++;
            }
          }
        }
        score += threat * 3; // favor higher level monsters
        score += hidden * 4; // favor cells next to unknowns
      } else {
        // this cell would kill us!
        continue;
      }
      if (score <= 0) continue;
      if (score > best_score) {
        best_score = score;
        same_score = 0;
        best_x = x;
        best_y = y;
      } else if (score == best_score) {
        same_score++;
        if (rnd_pick(&game->rnd, same_score)) {
          best_x = x;
          best_y = y;
        }
      }
    }
  }
  if (best_x >= 0) {
    return H_CLICK(best_x, best_y);
  }

  // not enough HP to make progress, so try and kill a wall
  if (game->hp > 0) {
    for (i32 y = 0, i = 0; y < BOARD_H; y++) {
      for (i32 x = 0; x < BOARD_W; x++, i++) {
        if (
          GET_STATUS(game->board[i]) != S_HIDDEN &&
          GET_TYPE(game->board[i]) == T_WALL
        ) {
          return H_CLICK(x, y);
        }
      }
    }
  }

  // no other action, so heal if possible
  if (game->exp >= max_exp(game)) {
    return H_LEVELUP();
  } else if (heal_x >= 0) {
    return H_CLICK(heal_x, heal_y);
  }

  // oh boy, I guess we're screwed

  return H_GIVEUP();
  #undef H_CLICK
  #undef H_NOTE
  #undef H_LEVELUP
  #undef H_GIVEUP
}

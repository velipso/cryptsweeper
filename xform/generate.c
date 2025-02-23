//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#include <stdio.h>
#include <stdbool.h>
#include "generate.h"

void print_board(const u8 *board) {
  for (i32 y = 0, k = 0; y < BOARD_H; y++) {
    for (i32 x = 0; x < BOARD_W; x++, k++) {
      i32 t = GET_TYPE(board[k]);
      switch (GET_STATUS(board[k])) {
        case 0: printf("\e[0m"); break;
        case 1: printf("\e[0;91m"); break; // red
        case 2: printf("\e[0;93m"); break; // yellow
        case 3: printf("\e[0;96m"); break; // cyan
      }
      switch (GET_TYPE(board[k])) {
        case T_EMPTY: printf(".. "); break;
        case T_LV1A : printf("1A "); break;
        case T_LV1B : printf("1B "); break;
        case T_LV2  : printf("2X "); break;
        case T_LV3A : printf("3A "); break;
        case T_LV3B : printf("3B "); break;
        case T_LV3C : printf("3C "); break;
        case T_LV4A : printf("4A "); break;
        case T_LV4B : printf("4B "); break;
        case T_LV4C : printf("4C "); break;
        case T_LV5A : printf("5A "); break;
        case T_LV5B : printf("5B "); break;
        case T_LV5C : printf("5C "); break;
        case T_LV6  : printf("6X "); break;
        case T_LV7  : printf("7X "); break;
        case T_LV8  : printf("8X "); break;
        case T_LV9  : printf("9X "); break;
        case T_LV10 : printf("10 "); break;
        case T_LV11 : printf("11 "); break;
        case T_LV13 : printf("13 "); break;
        case T_MINE : printf("() "); break;
        case T_WALL : printf("## "); break;
        case T_CHEST_HEAL: printf("[^ "); break;
        case T_CHEST_EYE2: printf("[@ "); break;
        case T_CHEST_EXP : printf("[+ "); break;
        default: printf("%02x ", t);
      }
    }
    printf("\n");
  }
  printf("\e[0m\n");
}

void generate_onlymines(u8 *board, i32 diff, struct rnd_st *rnd) {
  u8 minecount_table[] = { 16, 18, 20, 22, 26 };
  u8 minecount = minecount_table[diff];
  for (;;) {
    // place mines in empty board
    for (i32 i = 0; i < BOARD_SIZE; i++) {
      board[i] = 0;
    }
    for (i32 i = 0; i < minecount; i++) {
      for (;;) {
        i32 x = roll(rnd, BOARD_W);
        i32 y = roll(rnd, BOARD_H);
        i32 k = x + y * BOARD_W;
        if (board[k] == 0) {
          board[k] = T_MINE;
          break;
        }
      }
    }

    // calculate counts
    u8 threat[BOARD_SIZE] = {0};
    for (i32 y = 0, k = 0; y < BOARD_H; y++) {
      for (i32 x = 0; x < BOARD_W; x++, k++) {
        i32 t = 0;
        if (board[k] == 0) {
          t = count_threat(board, x, y) >> 8;
        }
        threat[k] = t == 0 ? 255 : t;
      }
    }

    // solve board
    u8 solution[BOARD_SIZE] = {0};
    for (;;) {
      bool any_dirty = false;
      for (i32 y = 0, k = 0; y < BOARD_H; y++) {
        for (i32 x = 0; x < BOARD_W; x++, k++) {
          bool dirty = false;
          if (threat[k] == 255) {
            // mine could potentially go here
            bool could = true;
            for (i32 dy = -1; dy <= 1 && could; dy++) {
              i32 by = y + dy;
              if (by < 0 || by >= BOARD_H) continue;
              for (i32 dx = -1; dx <= 1 && could; dx++) {
                if (dy == 0 && dx == 0) continue;
                i32 bx = x + dx;
                if (bx < 0 || bx >= BOARD_W) continue;
                i32 bk = bx + by * BOARD_W;
                could = threat[bk] > 0 || solution[bk];
              }
            }
            if (!could) {
              // erm, actually a mine *can't* go here
              dirty = true;
              threat[k] = 0;
            }
          } else if (threat[k] > 0) {
            // there's some number of mines touching this cell
            // where *could* the mine(s) be placed around this cell?
            i32 avail = 0;
            i32 ax[8], ay[8];
            for (i32 dy = -1; dy <= 1; dy++) {
              i32 by = y + dy;
              if (by < 0 || by >= BOARD_H) continue;
              for (i32 dx = -1; dx <= 1; dx++) {
                if (dy == 0 && dx == 0) continue;
                i32 bx = x + dx;
                if (bx < 0 || bx >= BOARD_W) continue;
                i32 bk = bx + by * BOARD_W;
                if (threat[bk] == 255) {
                  // mine could potentially be placed here
                  ax[avail] = bx;
                  ay[avail] = by;
                  avail++;
                }
              }
            }
            if (threat[k] == avail) {
              // only one available place the mine(s) could be, so place them!
              dirty = true;
              for (i32 a = 0; a < avail; a++) {
                solution[ax[a] + ay[a] * BOARD_W] = 1;
                threat[ax[a] + ay[a] * BOARD_W] = 0;
                for (i32 dy = -1; dy <= 1; dy++) {
                  i32 by = ay[a] + dy;
                  if (by < 0 || by >= BOARD_H) continue;
                  for (i32 dx = -1; dx <= 1; dx++) {
                    if (dy == 0 && dx == 0) continue;
                    i32 bx = ax[a] + dx;
                    if (bx < 0 || bx >= BOARD_W) continue;
                    i32 bk = bx + by * BOARD_W;
                    if (threat[bk] != 0 && threat[bk] != 255) threat[bk]--;
                  }
                }
              }
            }
          }
          if (dirty) {
            any_dirty = true;
          }
        }
      }
      if (!any_dirty) break;
    }

    // check if we solved it
    bool match = true;
    for (i32 i = 0; i < BOARD_SIZE && match; i++) {
      match = board[i] == (solution[i] ? T_MINE : T_EMPTY);
    }
    // one solution, so we're good!
    if (match) return;
    // otherwise, try again
  }
}

typedef bool (*istile_f)(u8 tile);

static bool istile_wall(u8 tile) {
  return GET_TYPE(tile) == T_WALL;
}

static bool istile_chest(u8 tile) {
  return IS_CHEST(tile);
}

static bool istile_chest_heal(u8 tile) {
  return GET_TYPE(tile) == T_CHEST_HEAL;
}

static bool istile_mine(u8 tile) {
  return GET_TYPE(tile) == T_MINE;
}

static bool istile_lv6(u8 tile) {
  return GET_TYPE(tile) == T_LV6;
}

static bool istile_lv5bplus(u8 tile) {
  tile = GET_TYPE(tile);
  return tile >= T_LV5B && tile <= T_LV13;
}

static i32 count_tiles(const u8 *board, i32 x, i32 y, i32 rad, istile_f istile) {
  i32 result = 0;
  bool diamond = false;
  i32 w = rad;
  if (rad < 0) {
    diamond = true;
    rad = -rad;
    w = 0;
  }
  for (i32 dy = -rad; dy <= rad; dy++) {
    i32 by = dy + y;
    if (by < 0 || by >= BOARD_H) goto next_dy;
    by *= BOARD_W;
    for (i32 dx = -w; dx <= w; dx++) {
      i32 bx = dx + x;
      if (bx < 0 || bx >= BOARD_W) continue;
      if (istile(board[bx + by])) {
        result++;
      }
    }
next_dy:
    if (diamond) {
      if (dy < 0) w++;
      else w--;
    }
  }
  return result;
}

static void place_random(u8 *board, struct rnd_st *rnd, i32 value, i32 count) {
  for (; count > 0; count--) {
    for (;;) {
      i32 x = roll(rnd, BOARD_W);
      i32 y = roll(rnd, BOARD_H);
      if (IS_EMPTYXY(board, x, y)) {
        board[x + y * BOARD_W] = value;
        break;
      }
    }
  }
}

static void copy_board(u8 *dst, const u8 *src) {
  for (i32 i = 0; i < BOARD_SIZE; i++) {
    dst[i] = src[i];
  }
}

void generate_normal(u8 *board, i32 diff, struct rnd_st *rnd) {
restart:
  for (i32 i = 0; i < BOARD_SIZE; i++) {
    board[i] = 0;
  }

  { // lv13 is in the center
    i32 x = roll(rnd, 2) + BOARD_CW - 1;
    i32 y = roll(rnd, 3) + BOARD_CH - 1;
    SET_TYPEXY(board, x, y, T_LV13);
    SET_STATUSXY(board, x, y, S_VISIBLE);
  }

  { // lv1b is always on the edge, surrounded by lv8's
    i32 x, y;
    switch (roll(rnd, 4)) {
      case 0: x = 0; y = roll(rnd, BOARD_H - 2) + 1; break;
      case 1: x = BOARD_W - 1; y = roll(rnd, BOARD_H - 2) + 1; break;
      case 2: x = roll(rnd, BOARD_W - 2) + 1; y = 0; break;
      case 3: x = roll(rnd, BOARD_W - 2) + 1; y = BOARD_H - 1; break;
    }
    SET_TYPEXY(board, x, y, T_LV1B);
    for (i32 dy = -1; dy <= 1; dy++) {
      i32 by = y + dy;
      if (by < 0 || by >= BOARD_H) continue;
      for (i32 dx = -1; dx <= 1; dx++) {
        i32 bx = x + dx;
        if ((dy == 0 && dx == 0) || bx < 0 || bx >= BOARD_W) continue;
        SET_TYPEXY(board, bx, by, T_LV8);
      }
    }
  }

  for (;;) { // lv10 is always in a corner
    i32 x, y;
    switch (roll(rnd, 4)) {
      case 0: x = 0; y = 0; break;
      case 1: x = BOARD_W - 1; y = 0; break;
      case 2: x = 0; y = BOARD_H - 1; break;
      case 3: x = BOARD_W - 1; y = BOARD_H - 1; break;
    }
    if (IS_EMPTYXY(board, x, y)) {
      SET_TYPEXY(board, x, y, T_LV10);
      break;
    }
  }

  for (;;) { // two lv9's are always mirrored
    i32 dx = roll(rnd, BOARD_CW - 2) + 1;
    i32 x1 = BOARD_CW - 1 - dx;
    i32 x2 = BOARD_CW + dx;
    i32 y = roll(rnd, BOARD_H);
    if (IS_EMPTYXY(board, x1, y) && IS_EMPTYXY(board, x2, y)) {
      SET_TYPEXY(board, x1, y, T_LV9);
      SET_TYPEXY(board, x2, y, T_LV9);
      break;
    }
  }

  for (i32 i = 0; i < 3; i++) { // walls come in pairs
    for (;;) {
      i32 x1, y1, x2, y2;
      if (roll(rnd, 2)) { // vertical
        x1 = x2 = roll(rnd, BOARD_W - 2) + 1;
        y1 = roll(rnd, BOARD_H - 1);
        y2 = y1 + 1;
      } else { // horizontal
        x1 = roll(rnd, BOARD_W - 1);
        x2 = x1 + 1;
        y1 = y2 = roll(rnd, BOARD_H - 2) + 1;
      }
      if (
        IS_EMPTYXY(board, x1, y1) &&
        IS_EMPTYXY(board, x2, y2) &&
        count_tiles(board, x1, y1, 2, istile_wall) == 0 &&
        count_tiles(board, x2, y2, 2, istile_wall) == 0
      ) {
        SET_TYPEXY(board, x1, y1, T_WALL);
        SET_TYPEXY(board, x2, y2, T_WALL);
        break;
      }
    }
  }

  for (i32 attempt = 0; ; attempt++) { // four lv7's in a box
    const i32 minw = 5, minh = 4;
    i32 w = roll(rnd, BOARD_W - 1 - minw) + minw;
    i32 h = roll(rnd, BOARD_H - 1 - minh) + minh;
    i32 x1 = roll(rnd, BOARD_W - w);
    i32 y1 = roll(rnd, BOARD_H - h);
    i32 x2 = x1 + w;
    i32 y2 = y1 + h;
    if (
      w >= minw &&
      h >= minh &&
      x1 >= 0 && x1 < BOARD_W &&
      y1 >= 0 && y1 < BOARD_H &&
      x2 >= 0 && x2 < BOARD_W &&
      y2 >= 0 && y2 < BOARD_H &&
      IS_EMPTYXY(board, x1, y1) &&
      IS_EMPTYXY(board, x2, y1) &&
      IS_EMPTYXY(board, x1, y2) &&
      IS_EMPTYXY(board, x2, y2)
    ) {
      SET_TYPEXY(board, x1, y1, T_LV7);
      SET_TYPEXY(board, x2, y1, T_LV7);
      SET_TYPEXY(board, x1, y2, T_LV7);
      SET_TYPEXY(board, x2, y2, T_LV7);
      break;
    }
    if (attempt > 100) goto restart;
  }

  for (i32 i = 0; i < 4; i++) { // four pairs of lv4's in formation
    for (i32 attempt = 0; ; attempt++) {
      i32 x1, y1, x2, y2, t;
      switch (roll(rnd, 3)) {
        case 0: // rooks
          t = T_LV4A;
          if (roll(rnd, 2)) { // vertical pair
            x1 = x2 = roll(rnd, BOARD_W - 2) + 1;
            y1 = roll(rnd, BOARD_H - 1);
            y2 = y1 + 1;
          } else { // horizontal pair
            x1 = roll(rnd, BOARD_W - 1);
            x2 = x1 + 1;
            y1 = y2 = roll(rnd, BOARD_H - 2) + 1;
          }
          break;
        case 1: // bishops
          t = T_LV4B;
          x1 = x2 = roll(rnd, BOARD_W - 2) + 1;
          y1 = roll(rnd, BOARD_H - 1);
          y2 = y1 + 1;
          if (roll(rnd, 2)) { // forward slash
            x1++;
          } else { // back slash
            x2++;
          }
          break;
        case 2: // knights
          t = T_LV4C;
          x1 = x2 = roll(rnd, BOARD_W - 4) + 2;
          y1 = y2 = roll(rnd, BOARD_H - 4) + 2;
          switch (roll(rnd, 8)) {
            case 0: x2 -= 2; y2 += 1; break;
            case 1: x2 -= 2; y2 -= 1; break;
            case 2: x2 -= 1; y2 += 2; break;
            case 3: x2 -= 1; y2 -= 2; break;
            case 4: x2 += 1; y2 += 2; break;
            case 5: x2 += 1; y2 -= 2; break;
            case 6: x2 += 2; y2 += 1; break;
            case 7: x2 += 2; y2 -= 1; break;
          }
          break;
      }
      if (
        x1 >= 0 && x1 < BOARD_W &&
        y1 >= 0 && y1 < BOARD_H &&
        x2 >= 0 && x2 < BOARD_W &&
        y2 >= 0 && y2 < BOARD_H &&
        IS_EMPTYXY(board, x1, y1) &&
        IS_EMPTYXY(board, x2, y2)
      ) {
        SET_TYPEXY(board, x1, y1, t);
        SET_TYPEXY(board, x2, y2, t);
        break;
      }
      if (attempt > 100) goto restart;
    }
  }

  for (i32 i = 0; i < 9; i++) { // place mines such that there aren't more than 4 threating a square
    for (i32 attempt = 0; ; attempt++) {
      i32 x = roll(rnd, BOARD_W);
      i32 y = roll(rnd, BOARD_H);
      if (
        IS_EMPTYXY(board, x, y) &&
        count_tiles(board, x - 1, y - 1, 1, istile_mine) < 4 &&
        count_tiles(board, x    , y - 1, 1, istile_mine) < 4 &&
        count_tiles(board, x + 1, y - 1, 1, istile_mine) < 4 &&
        count_tiles(board, x - 1, y    , 1, istile_mine) < 4 &&
        count_tiles(board, x + 1, y    , 1, istile_mine) < 4 &&
        count_tiles(board, x - 1, y + 1, 1, istile_mine) < 4 &&
        count_tiles(board, x    , y + 1, 1, istile_mine) < 4 &&
        count_tiles(board, x + 1, y + 1, 1, istile_mine) < 4
      ) {
        SET_TYPEXY(board, x, y, T_MINE);
        break;
      }
      if (attempt > 100) goto restart;
    }
  }

  for (i32 i = 0; i < 11; i++) { // place chests and lv6
    for (i32 attempt = 0; ; attempt++) {
      i32 x = roll(rnd, BOARD_W);
      i32 y = roll(rnd, BOARD_H);
      if (
        IS_EMPTYXY(board, x, y) &&
        count_tiles(board, x, y, 2, istile_chest) < 2
      ) {
        if (i < 6) {
          // place lv6 near chest
          i32 x2 = x + roll(rnd, 3) - 1;
          i32 y2 = y + roll(rnd, 3) - 1;
          if (
            (x2 == x && y2 == y) ||
            x2 < 0 ||
            x2 >= BOARD_W ||
            y2 < 0 ||
            y2 >= BOARD_H ||
            !IS_EMPTYXY(board, x2, y2) ||
            count_tiles(board, x, y, 1, istile_lv6)
          ) {
            // can't place here :-(
            continue;
          }
          SET_TYPEXY(board, x2, y2, T_LV6);
        }
        SET_TYPEXY(board, x, y, i == 0 ? T_CHEST_EYE2 : i < 4 ? T_CHEST_EXP : T_CHEST_HEAL);
        break;
      }
      if (attempt > 100) goto restart;
    }
  }

  // last pieces
  u8 copy[BOARD_SIZE];
  copy_board(copy, board);
  for (i32 attempt = 0; ; attempt++) {
    // place the rest randomly
    place_random(board, rnd, T_LV5A, 8); // slime
    place_random(board, rnd, T_LV5B, 1); // spiderking
    place_random(board, rnd, T_LV5C, 2); // gazer
    place_random(board, rnd, T_LV1A, 12);
    place_random(board, rnd, T_LV2, 11);
    place_random(board, rnd, T_LV3A, 4);
    place_random(board, rnd, T_LV3B, 2);
    place_random(board, rnd, T_LV3C, 3);
    place_random(board, rnd, T_LV11, 1);
    // final placement is the starting location, which should reveal certain things
    i32 bx = -1;
    i32 by = -1;
    i32 found = 0;
    for (i32 y = 2; y < BOARD_H - 2; y++) {
      for (i32 x = 2; x < BOARD_W - 2; x++) {
        if (
          IS_EMPTY(board[x + y * BOARD_W]) &&
          // don't reveal spiderking or higher
          count_tiles(board, x, y, -2, istile_lv5bplus) == 0 &&
          // exactly one wall
          count_tiles(board, x, y, -2, istile_wall) == 1 &&
          // exactly one chest with healing in it
          count_tiles(board, x, y, -2, istile_chest) == 1 &&
          count_tiles(board, x, y, -2, istile_chest_heal) == 1 &&
          // no mines
          count_tiles(board, x, y, -2, istile_mine) == 0
        ) {
          if (rnd_pick(rnd, found)) {
            bx = x;
            by = y;
          }
          found++;
        }
      }
    }
    if (found > 0) {
      SET_TYPEXY(board, bx, by, T_ITEM_EYE);
      SET_STATUSXY(board, bx, by, S_PRESSED);
      break;
    }
    if (attempt > 100) goto restart;
    // we've come so far... let's not start completely over, just try again
    copy_board(board, copy);
  }
}

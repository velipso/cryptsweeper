//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#include "generate.h"
#include "../src/game.h"

void print_board(u8 *board) {
  for (i32 y = 0, k = 0; y < BOARD_H; y++) {
    for (i32 x = 0; x < BOARD_W; x++, k++) {
      i32 t = GET_TYPE(board[k]);
      switch (GET_STATUS(board[k])) {
        case 0: printf("\e[0m"); break;
        case 1: printf("\e[0;91m"); break; // red
        case 2: printf("\e[0;93m"); break; // yellow
        case 3: printf("\e[0;96m"); break; // cyan
      }
      if (t == T_EMPTY) printf(".. ");
      else printf("%02x ", t);
    }
    printf("\n");
  }
  printf("\e[0m\n");
}

static void generate_onlymines(u8 *board, i32 diff, struct rnd_st *rnd) {
  u8 minecount_table[] = { 16, 18, 20, 22, 26 };
  u8 minecount = minecount_table[diff];
  for (i32 i = 0; i < minecount; i++) {
    for (;;) {
      i32 x = roll(rnd, BOARD_W);
      i32 y = roll(rnd, BOARD_H);
      if (x == BOARD_CW && y == BOARD_CH) continue; // never place directly under starting position
      i32 k = x + y * BOARD_W;
      if (board[k] == 0) {
        board[k] = T_MINE;
        break;
      }
    }
  }

  // calculate counts
  u8 threat[BOARD_SIZE];
  for (i32 y = 0, k = 0; y < BOARD_H; y++) {
    for (i32 x = 0; x < BOARD_W; x++, k++) {
      threat[k] = count_threat(board, x, y) >> 8;
    }
  }
  print_board(threat);
}

void generate(u32 seed, u8 *out) {
  printf("seed = %08X\n", seed);
  struct rnd_st rnd;
  rnd_seed(&rnd, seed);
  u8 board[BOARD_SIZE] = {0};
  generate_onlymines(board, 0, &rnd);
  print_board(board);
}

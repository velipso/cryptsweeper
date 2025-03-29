//
// cryptsweeper - fight the graveyard monsters and stop death
// by Pocket Pulp (@velipso), https://pulp.biz
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#include "sfx.h"

void sfx_bump() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("bump");
  snd_play_wav(sfx, 16, 10);
}

void sfx_slideup() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("slideup");
  snd_play_wav(sfx, 8, 10);
}

void sfx_slidedown() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("slidedown");
  snd_play_wav(sfx, 8, 10);
}

void sfx_blip() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("blip");
  snd_play_wav(sfx, 10, 10);
}

void sfx_accept() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("accept");
  snd_play_wav(sfx, 12, 10);
}

void sfx_reject() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("reject");
  snd_play_wav(sfx, 14, 10);
}

void sfx_book() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("book");
  snd_play_wav(sfx, 10, 10);
}

void sfx_swipeup() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("swipeup");
  snd_play_wav(sfx, 4, 10);
}

void sfx_swipedown() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("swipedown");
  snd_play_wav(sfx, 3, 10);
}

void sfx_eye() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("eye");
  snd_play_wav(sfx, 14, 10);
}

void sfx_chest() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("chest");
  snd_play_wav(sfx, 12, 10);
}

void sfx_wall() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("wall");
  snd_play_wav(sfx, 12, 10);
}

void sfx_exp1() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("exp1");
  snd_play_wav(sfx, 12, 10);
}

void sfx_exp2() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("exp2");
  snd_play_wav(sfx, 12, 10);
}

void sfx_grunt1() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("grunt1");
  snd_play_wav(sfx, 12, 10);
}

void sfx_grunt2() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("grunt2");
  snd_play_wav(sfx, 12, 10);
}

void sfx_grunt3() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("grunt3");
  snd_play_wav(sfx, 12, 10);
}

void sfx_grunt4() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("grunt4");
  snd_play_wav(sfx, 12, 10);
}

void sfx_grunt5() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("grunt5");
  snd_play_wav(sfx, 12, 10);
}

void sfx_grunt6() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("grunt6");
  snd_play_wav(sfx, 12, 10);
}

void sfx_grunt7() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("grunt7");
  snd_play_wav(sfx, 12, 10);
}

void sfx_exit() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("exit");
  snd_play_wav(sfx, 12, 10);
}

void sfx_heart() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("heart");
  snd_play_wav(sfx, 12, 10);
}

void sfx_youdie() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("youdie");
  snd_play_wav(sfx, 16, 10);
}

void sfx_levelup() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("levelup");
  snd_play_wav(sfx, 12, 10);
}

void sfx_mine() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("mine");
  snd_play_wav(sfx, 14, 10);
}

void sfx_dirt() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("dirt");
  snd_play_wav(sfx, 10, 10);
}

void sfx_lava() {
  static int sfx = -2;
  if (sfx == -2)
    sfx = snd_find_wav("lava");
  snd_play_wav(sfx, 10, 10);
}

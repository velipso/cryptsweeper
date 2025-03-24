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

//
// cryptsweeper - fight the graveyard monsters and stop death
// by Pocket Pulp (@velipso), https://pulp.biz
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#define DUTY_COUNT        8
#define MAX_PHASE_BITS    11
#define MAX_PHASE         (1 << MAX_PHASE_BITS)
#define PITCH_DIV_BITS    4 /* how many freqs between notes (2^n) */
#define MAX_PHASE_Q_BITS  (MAX_PHASE_BITS + 5)
#define MAX_RND_SAMPLE    (1 << 15)
#define PI                3.14159265358979323846

int snd_main(int argc, const char **argv);

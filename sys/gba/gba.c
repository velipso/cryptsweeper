//
// cryptsweeper - fight the graveyard monsters and stop death
// by Pocket Pulp (@velipso), https://pulp.biz
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#include "gba.h"
#include "reg.h"
#include <stdarg.h>

extern void sys__irq_init();
extern void (*sys__irq_vblank)();
extern void (*sys__irq_timer1)();
extern void sys__snd_timer1_handler();
extern void sys__snd_frame();

struct snd_st g_snd;

static void (*g_vblank)();

static void _sys_snd_init();
static void _save_init();

BINFILE(snd_tempo_bin);
BINFILE(snd_wavs_bin);
BINFILE(snd_offsets_bin);
BINFILE(snd_sizes_bin);
BINFILE(snd_names_txt);

static struct {
  u32 region_count;
  u32 cart_size;
  struct {
    u32 sectors;
    u32 size;
  } regions[4];
} g_save;

#define ROM_SET(x, y) do { ((volatile u16 *)0x08000000)[x] = y; __asm("nop"); } while (0)
#define ROM_GET(x)    (((volatile u16 *)0x08000000)[x])

void sys_init() {
  _save_init();
  sys__irq_init();
  _sys_snd_init();
}

bool sys_mGBA() {
  volatile u16 *reg = (u16 *)0x4fff780;
  *reg = 0xc0de;
  return *reg == 0x1dea;
}

SECTION_IWRAM_ARM static void _sys_wrap_vblank() {
  // allow re-entrant IRQs so timer1 for snd is handled
  REG_IME = 1;
  if (g_vblank)
    g_vblank();
  sys__snd_frame();
}

void sys_set_vblank(void (*irq_vblank_handler)()) {
  g_vblank = irq_vblank_handler;
}

SECTION_IWRAM_ARM void sys_nextframe() {
  __asm__("swi #0x050000" ::: "r0", "r1", "r2", "r3", "r12", "lr", "memory", "cc");
}

static void _sys_snd_init() {
  REG_IME = 0;
  // enable vblank handler
  REG_DISPSTAT |= 0x0008;
  REG_IE |= 1;
  g_vblank = NULL;
  sys__irq_vblank = _sys_wrap_vblank;
  // enable timer1 handler
  sys__irq_timer1 = sys__snd_timer1_handler;
  // clear struct
  memset32(&g_snd, 0, sizeof(g_snd));
  // enable timer1
  REG_IE |= 0x10;
  // setup timers
  REG_TM0CNT = 0;
  REG_TM1CNT = 0;
  // setup timer0 - drives the sample rate
  // cpuHz      = 2^24
  // sampleRate = 2^15
  // timer0Wait = cpuHz / sampleRate = 2^9 = 0x200
  // timer0Res  = 1 cycle
  // timer0Wait / timer0Res = 0x200
  REG_TM0D = 0x10000 - 0x200;
  // setup timer1 - drives the DMA buffer cycling
  // bufferSize = 0x260
  // timer1Wait = bufferSize * timer0Wait = 311296 cycles
  // timer1Res  = 64 cycles
  // timer1Wait / timer1Res = 0x1300
  REG_TM1D = 0x10000 - 0x1300;
  REG_IME = 1;
  // turn sound chip on
  REG_SOUNDCNT_X = 0x0080;
  // set sound to use FIFO A
  REG_SOUNDCNT_H = 0x0b0f;
  // set DMA1 destination to FIFO A
  REG_DMA1DAD = (u32)&REG_FIFO_A;
  // point DMA1 to buffer1
  REG_DMA1SAD = (u32)g_snd.buffer1;
  // enable DMA1
  REG_DMA1CNT_H = 0xb640;
  // save alt buffers for next render
  g_snd.buffer_addr[0] = g_snd.buffer1;
  g_snd.buffer_addr[1] = g_snd.buffer2;
  g_snd.buffer_addr[2] = g_snd.buffer3;
  g_snd.next_buffer_index = 4;
  // start timer0
  REG_TM0CNT = 0x0080;
  // start timer1
  REG_TM1CNT = 0x00c1;
}

void snd_load_song(const void *song_base, int sequence) {
  int volume = g_snd.synth.volume;
  memset32(&g_snd.synth, 0, sizeof(struct snd_synth_st));
  g_snd.synth.volume = volume;
  g_snd.synth.song_base = song_base;
  g_snd.synth.sequence = sequence;
  // set pattern pointer to sequence's first pattern
  const struct snd_song_st *song = song_base;
  int seq_offset = *((int *)(song_base + song->seq_table_offset + sequence * 4));
  const struct snd_songseq_st *songseq = song_base + seq_offset;
  u16 patterns = songseq->patterns[0];
  u32 pat_offset = *((u32 *)(song_base + song->pat_table_offset + patterns * 4));
  g_snd.synth.pat = song_base + pat_offset;

  // set tempo to index 0
  const u16 *tempo_table = BINADDR(snd_tempo_bin);
  g_snd.synth.tick_start = tempo_table[0];
  g_snd.synth.tick_left = 0;

  // initialize channel volume
  for (int i = 0; i < SND_MAX_CHANNELS; i++)
    g_snd.synth.channel[i].chan_volume = i < song->channel_count ? 8 : 0;
}

void snd_set_master_volume(int v) {
  g_snd.master_volume = v < 0 ? 0 : v > 16 ? 16 : v;
}

void snd_set_song_volume(int v) {
  g_snd.synth.volume = v < 0 ? 0 : v > 16 ? 16 : v;
}

void snd_set_sfx_volume(int v) {
  g_snd.sfx_volume = v < 0 ? 0 : v > 16 ? 16 : v;
}

int snd_find_wav(const char *name) {
  const char *start = BINADDR(snd_names_txt);
  int size = BINSIZE(snd_names_txt);
  int state = 0;
  int index = 0;
  int m = 0;
  int matching = 0;
  for (int i = 0; i < size; i++) {
    char ch = start[i];
    if (ch == 0)
      return -1;
    switch (state) {
      case 0:
        if (ch != '\n') {
          matching = ch == name[0];
          m = 1;
          state = 1;
        }
        break;
      case 1:
        if (ch == '\n') {
          if (matching && name[m] == 0)
            return index;
          index++;
          state = 0;
        } else if (matching) {
          matching = ch == name[m++];
        }
        break;
    }
  }
  return -1;
}

bool snd_play_wav(int wav_index, int volume, int priority) {
  if (wav_index < 0)
    return false;
  // look for either an empty slot or the lowest priority slot
  int slot;
  int best_slot = 0;
  int best_priority = g_snd.sfx[0].priority;
  for (slot = 0; slot < SND_MAX_SFX; slot++) {
    if (!g_snd.sfx[slot].wav_base)
      goto place_in_slot;
    if (g_snd.sfx[slot].priority < best_priority) {
      best_priority = g_snd.sfx[slot].priority;
      best_slot = slot;
    }
  }
  if (priority <= best_priority)
    return false;
  slot = best_slot;
place_in_slot:
  const u8 *wavs = BINADDR(snd_wavs_bin);
  const u32 *offsets = BINADDR(snd_offsets_bin);
  const u32 *sizes = BINADDR(snd_sizes_bin);
  g_snd.sfx[slot].wav_base = &wavs[offsets[wav_index]];
  g_snd.sfx[slot].wav_volume = volume;
  g_snd.sfx[slot].samples_left = sizes[wav_index];
  g_snd.sfx[slot].priority = priority;
  return true;
}

SECTION_EWRAM_THUMB static void _save_init() {
  g_save.region_count = 0;

  // reset
  ROM_SET(0, 0xf0);
  // enter CFI
  ROM_SET(0x55, 0x98);
  u32 Q = ROM_GET(0x10);
  u32 R = ROM_GET(0x11);
  u32 Y = ROM_GET(0x12);

  if (Q == 'Q' && R == 'R' && Y == 'Y') {
    // CFI mode!
    g_save.region_count = ROM_GET(0x2c);
    if (g_save.region_count < 1 || g_save.region_count > 4) {
      // something is wrong, fallback to SRAM
      g_save.region_count = 0;
    } else {
      g_save.cart_size = 0;
      for (i32 i = 0; i < g_save.region_count; i++) {
        g_save.regions[i].sectors = (ROM_GET(0x2d + i * 4) | (ROM_GET(0x2e + i * 4) << 8)) + 1;
        g_save.regions[i].size = (ROM_GET(0x2f + i * 4) | (ROM_GET(0x30 + i * 4) << 8)) << 8;
        g_save.cart_size += g_save.regions[i].sectors * g_save.regions[i].size;
      }
    }
  }

  // reset
  ROM_SET(0, 0xf0);
}

SECTION_EWRAM_THUMB static u32 save_calcflashsectors(u16 **primary, u16 **backup, u16 *write) {
  // returns:
  //   primary - primary sector address (i.e., last written save)
  //   backup  - backup sector address (i.e., write here next)
  //   write   - next priority to write to backup[sz-1] in order to make it primary
  //   returns - size of sector, in 16-bit words (*not* bytes)
  u32 last_sector_size = g_save.regions[g_save.region_count - 1].size;
  u16 *s1 = (u16 *)(((u8 *)0x08000000) + g_save.cart_size - last_sector_size);
  u16 *s2 = (u16 *)(((u8 *)s1) - last_sector_size);
  u16 sz = last_sector_size >> 1; // number of 16-bit values per sector
  u16 s1code = s1[sz - 1];
  u16 s2code = s2[sz - 1];
  #define ROCK      0xa5
  #define PAPER     0xa9
  #define SCISSORS  0xaa
  if (s1code == ROCK) {
    if (s2code == PAPER) {
      if (primary) *primary = s2;
      if (backup) *backup = s1;
      if (write) *write = SCISSORS;
      return sz;
    } else if (s2code == SCISSORS) {
      if (primary) *primary = s1;
      if (backup) *backup = s2;
      if (write) *write = PAPER;
      return sz;
    }
  } else if (s1code == PAPER) {
    if (s2code == SCISSORS) {
      if (primary) *primary = s2;
      if (backup) *backup = s1;
      if (write) *write = ROCK;
      return sz;
    } else if (s2code == ROCK) {
      if (primary) *primary = s1;
      if (backup) *backup = s2;
      if (write) *write = SCISSORS;
      return sz;
    }
  } else if (s1code == SCISSORS) {
    if (s2code == ROCK) {
      if (primary) *primary = s2;
      if (backup) *backup = s1;
      if (write) *write = PAPER;
      return sz;
    } else if (s2code == PAPER) {
      if (primary) *primary = s1;
      if (backup) *backup = s2;
      if (write) *write = ROCK;
      return sz;
    }
  }
  // otherwise, we're corrupt
  if (s1code == ROCK || s1code == PAPER || s1code == SCISSORS) {
    if (primary) *primary = s1;
    if (backup) *backup = s2;
    if (write) *write = s1code == ROCK ? PAPER : s1code == PAPER ? SCISSORS : ROCK;
    return sz;
  } else {
    if (primary) *primary = s2;
    if (backup) *backup = s1;
    if (write) *write = s2code == ROCK ? PAPER : s2code == PAPER ? SCISSORS : ROCK;
    return sz;
  }
  #undef ROCK
  #undef PAPER
  #undef SCISSORS
}

SECTION_EWRAM_THUMB void save_write(const void *src, u32 size) {
  if (g_save.region_count == 0) {
    // SRAM
    memcpy8((void *)0x0e000000, src, size);
  } else {
    // Flash (assumes one sector for save)

    // disable access to cart
    void (*old_vblank)() = g_vblank;
    g_vblank = NULL;
    int old_volume = g_snd.master_volume;
    g_snd.master_volume = 0;

    volatile u16 *backup;
    u16 write;
    u32 sz = save_calcflashsectors(NULL, (u16 **)&backup, &write);

    // erase sector
    ROM_SET(0, 0xf0);
    ROM_SET(0x555, 0xaa);
    ROM_SET(0x2aa, 0x55);
    ROM_SET(0x555, 0x80);
    ROM_SET(0x555, 0xaa);
    ROM_SET(0x2aa, 0x55);
    backup[0] = 0x30;

    // wait for erase to finish
    for (;;) {
      __asm("nop");
      if (backup[0] == 0xffff) {
        break;
      }
    }
    ROM_SET(0, 0xf0);

    // program sector
    #define PROGRAM(pa, pd)  do {  \
        ROM_SET(0x555, 0xaa);      \
        ROM_SET(0x2aa, 0x55);      \
        ROM_SET(0x555, 0xa0);      \
        u16 v = pd;                \
        backup[pa] = v;            \
        for (;;) {                 \
          __asm("nop");            \
          if (backup[pa] == v) {   \
            break;                 \
          }                        \
        }                          \
      } while (0)
    const u16 *src16 = src;
    size >>= 1;
    for (i32 i = 0; i < size; i++) {
      PROGRAM(i, src16[i]);
    }
    PROGRAM(sz - 1, write); // write the final word that flips this sector to primary
    #undef PROGRAM

    // reset
    ROM_SET(0, 0xf0);

    // enable access to cart
    g_snd.master_volume = old_volume;
    g_vblank = old_vblank;
  }
}

SECTION_EWRAM_THUMB void save_read(void *dst, u32 size) {
  if (g_save.region_count == 0) {
    // SRAM
    memcpy8(dst, (void *)0x0e000000, size);
  } else {
    // Flash (assumes one sector for save)
    u16 *primary;
    save_calcflashsectors(&primary, NULL, NULL);
    memcpy32(dst, primary, size);
  }
}

#ifdef SYS_PRINT
void sys_print(const char *fmt, ...) {
  if (!sys_mGBA()) {
    return;
  }
  va_list args;
  va_start(args, fmt);
  i32 len = 0;
  volatile u8 *here = &REG_DEBUG_STRING;

  #define putchar(c)  do {  \
      *here = c;            \
      here++;               \
      len++;                \
    } while (0)

  for (const char *p = fmt; *p != '\0' && len < 250; p++) {
    char ch = *p;
    if (ch == '%') {
      p++;
      ch = *p;
      switch (ch) {
        case '%':
          putchar('%');
          break;
        case 'x': {
          u32 val = va_arg(args, u32);
          const char *hex = "0123456789abcdef";
          if (len < 244) {
            putchar('0');
            putchar('x');
            if (val < 256) {
              for (i32 i = 4; i >= 0; i -= 4) {
                putchar(hex[(val >> i) & 0xf]);
              }
            } else if (val < 65536) {
              for (i32 i = 12; i >= 0; i -= 4) {
                putchar(hex[(val >> i) & 0xf]);
              }
            } else {
              for (i32 i = 28; i >= 0; i -= 4) {
                putchar(hex[(val >> i) & 0xf]);
              }
            }
          }
          break;
        }
        default:
          putchar('%');
          putchar(ch);
          break;
      }
    } else {
      putchar(ch);
    }
  }
  va_end(args);

  #undef putchar

  *here = 0;
  REG_DEBUG_FLAGS = 1 | 0x100;
}
#endif // SYS_PRINT

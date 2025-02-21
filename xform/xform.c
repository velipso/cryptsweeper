//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#include "snd.h"
#include "../src/game.h"

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;

static void print_usage() {
  printf(
    "Usage:\n"
    "  xform <command> [options...]\n"
    "\n"
    "Commands:\n"
    "\n"
    "  snd <more...>\n"
    "    Interact with the sound subsystem\n"
    "\n"
    "  fix <input.gba>\n"
    "    Patches the ROM header and pads to power of 2\n"
    "\n"
    "  palette256 <output.bin> [input1.png input2.png ...]\n"
    "    Create a 256 color palette from the input files\n"
    "\n"
    "  brightness <output.bin> <palette.bin> <count> <baseline> <gamma> <ex> \\\n"
    "    <skipdark> <skiplight>\n"
    "    Create <count> palettes on a brightness curve\n"
    "\n"
    "  copy256 <input.png> <palette.bin> <output.bin>\n"
    "    Outputs image directly with 1 byte per pixel\n"
    "\n"
    "  copy8x8 <input.png> <palette.bin> <output.bin>\n"
    "    Outputs 8x8 tiles from 8x8 source image\n"
    "\n"
    "  levels <count> <seed> <output.bin>\n"
    "    Generate starter levels\n"
  );
}

// align files to 4 bytes... required to keep linker in alignment (???)
static void fclose4(FILE *fp) {
  long pos = ftell(fp);
  int pad = (pos % 4) == 0 ? 0 : 4 - (pos % 4);
  for (int i = 0; i < pad; i++)
    fputc(0, fp);
  fclose(fp);
}

static int fix_rom(const char *file) {
  printf("Fixing: %s\n", file);
  FILE *fp = fopen(file, "r+b");

  { // fix the header CRC
    int8_t header[0xc0];
    if (fread(&header, sizeof(header), 1, fp) != 1) {
      fclose(fp);
      fprintf(stderr, "\nFailed to read header: %s\n", file);
      return 1;
    }
    int8_t crc = 0;
    for (int n = 0xa0; n < 0xbd; n++) {
      crc += header[n];
    }
    header[0xbd] = -(0x19 + crc);
    fseek(fp, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, fp);
  }

  { // pad with 0xff to the next power of 2
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    printf("Original ROM size: %ldK\n", size >> 10);

    // round up to next power of 2
    long p = size;
    p--;
    p |= p >> 1;
    p |= p >> 2;
    p |= p >> 4;
    p |= p >> 8;
    p |= p >> 16;
    p++;

    while (size < p) {
      fputc(0xff, fp);
      size++;
    }
  }

  fclose(fp);
  printf("ROM fixed!\n");
  return 0;
}

static u16 *readpal(const char *file, int *entries) {
  FILE *fp = fopen(file, "rb");
  if (fp == NULL) {
    fprintf(stderr, "\nFailed to read palette: %s\n", file);
    exit(1);
  }
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  u8 *data = malloc(size);
  *entries = size / 2;
  fread(data, size, 1, fp);
  fclose(fp);
  return (u16 *)data;
}

static int findpal(u32 color, const u16 *palette, int maxpal) {
  int r = (color >> 0) & 0xff;
  int g = (color >> 8) & 0xff;
  int b = (color >> 16) & 0xff;
  int a = (color >> 24) & 0xff;
  if (a != 0xff) {
    return 0;
  }
  u16 rgb = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10);
  for (int i = 1; i < maxpal; i++) {
    if (palette[i] == rgb) {
      return i;
    }
  }
  return -1;
}

static int brightness_scale(
  double c,
  int i,
  int count,
  int baseline,
  double gamma,
  double ex
) {
  double b;
  if (i < baseline) {
    b = ((double)i) / baseline;
  } else if (i == baseline) {
    b = 1.0;
  } else {
    b = 1.0 + ((double)(i - baseline)) / (count - baseline - 1);
    c += (b - 1.0) / pow(2, ex);
  }
  c = pow(pow(c, 1.0 / gamma) * pow(b, ex), gamma);
  if (c < 0) {
    c = 0;
  } else if (c > 1) {
    c = 1;
  }
  return ((int)(c * 0x1f)) & 0x1f;
}

static int brightness(
  const char *output,
  const char *palette,
  int count,
  int baseline,
  double gamma,
  double ex,
  int sd,
  int sl
) {
  int maxpal;
  u16 *pal = readpal(palette, &maxpal);
  FILE *fp = fopen(output, "wb");
  if (!fp) {
    fprintf(stderr, "\nFailed to write: %s\n", output);
    free(pal);
    return 1;
  }

  for (int i = 0; i < count; i++) {
    for (int p = 0; p < maxpal; p++) {
      double r = (pal[p] & 0x1f) / 31.0;
      double g = ((pal[p] >> 5) & 0x1f) / 31.0;
      double b = ((pal[p] >> 10) & 0x1f) / 31.0;

      int ri = brightness_scale(r, i + sd, count + sd + sl, baseline + sd, gamma, ex);
      int gi = brightness_scale(g, i + sd, count + sd + sl, baseline + sd, gamma, ex);
      int bi = brightness_scale(b, i + sd, count + sd + sl, baseline + sd, gamma, ex);

      u16 rgb = ri | (gi << 5) | (bi << 10);
      fwrite(&rgb, 2, 1, fp);
    }
  }

  fclose4(fp);
  free(pal);
  return 0;
}

static int copy256(const char *input, const char *palette, const char *output) {
  int maxpal;
  u16 *pal = readpal(palette, &maxpal);
  FILE *fp = fopen(input, "rb");
  if (fp == NULL) {
    fprintf(stderr, "\nFailed to read: %s\n", input);
    return 1;
  }
  int width, height;
  u32 *data = (u32 *)stbi_load_from_file(fp, &width, &height, NULL, 4);
  fclose(fp);

  fp = fopen(output, "wb");
  if (fp == NULL) {
    fprintf(stderr, "\nFailed to write: %s\n", output);
    return 1;
  }

  u8 *dst = malloc(sizeof(u8) * width * height);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int k = x + y * width;
      int c = findpal(data[k], pal, maxpal);
      if (c < 0) {
        fprintf(stderr, "\nWARNING: Cannot find color at (%d, %d) in %s\n", x, y, input);
        c = 0;
      }
      dst[k] = c;
    }
  }
  fwrite(dst, sizeof(u8), width * height, fp);
  free(dst);

  fclose4(fp);
  stbi_image_free(data);
  free(pal);
  return 0;
}

static int copy8x8(const char *input, const char *palette, const char *output) {
  int maxpal;
  u16 *pal = readpal(palette, &maxpal);
  FILE *fp = fopen(input, "rb");
  if (fp == NULL) {
    fprintf(stderr, "\nFailed to read: %s\n", input);
    return 1;
  }
  int width, height;
  u32 *data = (u32 *)stbi_load_from_file(fp, &width, &height, NULL, 4);
  fclose(fp);

  fp = fopen(output, "wb");
  if (fp == NULL) {
    fprintf(stderr, "\nFailed to write: %s\n", output);
    return 1;
  }

  int tcx = width / 8;
  int tcy = height / 8;
  for (int ty = 0; ty < tcy; ty++) {
    for (int tx = 0; tx < tcx; tx++) {
      u8 src[64] = {0};

      // load 8x8 tile into src
      for (int py = 0; py < 8; py++) {
        int y = ty * 8 + py;
        for (int px = 0; px < 8; px++) {
          int x = tx * 8 + px;
          int k = x + y * width;
          int c = findpal(data[k], pal, maxpal);
          if (c < 0) {
            fprintf(stderr, "\nWARNING: Cannot find color at (%d, %d) in %s\n", x, y, input);
            c = 0;
          }
          src[px + py * 8] = c;
        }
      }

      // write results
      fwrite(src, sizeof(src), 1, fp);
    }
  }

  fclose4(fp);
  stbi_image_free(data);
  free(pal);
  return 0;
}

static int addpalette(const char *input, u16 *palette, int *nextpal, int maxpal) {
  FILE *fp = fopen(input, "rb");
  int width, height;
  u32 *data = (u32 *)stbi_load_from_file(fp, &width, &height, NULL, 4);
  if (data == NULL) {
    fprintf(stderr, "\nFailed to read: %s\n", input);
    return 1;
  }
  for (int i = 0; i < width * height; i++) {
    u32 color = data[i];
    int r = (color >> 0) & 0xff;
    int g = (color >> 8) & 0xff;
    int b = (color >> 16) & 0xff;
    int a = (color >> 24) & 0xff;
    if (a != 0xff) {
      continue;
    }
    u16 rgb = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10);
    bool found = false;
    for (int j = 1; j < *nextpal && !found; j++) {
      found = palette[j] == rgb;
    }
    if (!found) {
      if (*nextpal >= maxpal) {
        fprintf(stderr, "\nOut of palette space!\n");
        return 1;
      }
      palette[*nextpal] = rgb;
      *nextpal = *nextpal + 1;
    }
  }
  stbi_image_free(data);
  return 0;
}

static int levels(int count, int seed, const char *output) {
  FILE *fp = fopen(output, "wb");
  if (fp == NULL) {
    fprintf(stderr, "\nFailed to write: %s\n", output);
    return 1;
  }
  u8 *levels = calloc(count, 128);
  struct levelgen_st ctx;
  for (int i = 0; i < count; i++, seed++) {
    if ((i % 50) == 49 || i == count - 1) {
      printf("generating level %d/%d\n", i + 1, count);
    }
    levelgen_seed(&ctx, seed);
    levelgen_stage1(&ctx);
    for (int j = 0; j < BOARD_SIZE; j++) {
      levels[i * 128 + j] = ctx.board[j];
    }
  }
  fwrite(levels, 128, count, fp);
  fclose4(fp);
  free(levels);
  return 0;
}

int main(int argc, const char **argv) {
  if (argc < 2) {
    print_usage();
    return 0;
  }

  if (strcmp(argv[1], "fix") == 0) {
    if (argc != 3) {
      print_usage();
      fprintf(stderr, "\nExpecting: fix <input.gba>\n");
      return 1;
    }
    return fix_rom(argv[2]);
  } else if (strcmp(argv[1], "palette256") == 0) {
    if (argc < 3) {
      print_usage();
      fprintf(stderr, "\nExpecting: palette256 <output.bin> [input.png...]\n");
      return 1;
    }
    u16 palette[256] = {0};
    int nextpal = 1;
    for (int i = 3; i < argc; i++) {
      int res = addpalette(argv[i], palette, &nextpal, 256);
      if (res != 0) {
        return res;
      }
    }
    printf("Palette size: %d colors\n", nextpal);
    FILE *fp = fopen(argv[2], "wb");
    if (fp == NULL) {
      fprintf(stderr, "\nFailed to write: %s\n", argv[2]);
      return 1;
    }
    fwrite(palette, sizeof(palette), 1, fp);
    fclose4(fp);
    return 0;
  } else if (strcmp(argv[1], "brightness") == 0) {
    if (argc != 10) {
      print_usage();
      fprintf(
        stderr,
        "\nExpecting brightness <output.bin> <palette.bin> <count> <baseline> <gamma> <ex> "
        "<skipdark> <skiplight>\n"
      );
      return 1;
    }
    return brightness(
      argv[2],
      argv[3],
      atoi(argv[4]),
      atoi(argv[5]),
      strtod(argv[6], NULL),
      strtod(argv[7], NULL),
      atoi(argv[8]),
      atoi(argv[9])
    );
  } else if (strcmp(argv[1], "copy256") == 0) {
    if (argc != 5) {
      print_usage();
      fprintf(stderr, "\nExpecting copy256 <input.png> <palette.bin> <output.bin>\n");
      return 1;
    }
    return copy256(argv[2], argv[3], argv[4]);
  } else if (strcmp(argv[1], "copy8x8") == 0) {
    if (argc != 5) {
      print_usage();
      fprintf(stderr, "\nExpecting copy8x8 <input.png> <palette.bin> <output.bin>\n");
      return 1;
    }
    return copy8x8(argv[2], argv[3], argv[4]);
  } else if (strcmp(argv[1], "levels") == 0) {
    if (argc != 5) {
      print_usage();
      fprintf(stderr, "\nExpecting levels <count> <seed> <output.bin>\n");
      return 1;
    }
    return levels(atoi(argv[2]), atoi(argv[3]), argv[4]);
  } else if (strcmp(argv[1], "snd") == 0) {
    return snd_main(argc - 2, &argv[2]);
  } else {
    print_usage();
    fprintf(stderr, "\nUnknown command: %s\n", argv[1]);
    return 1;
  }

  return 0;
}

//
// cryptsweeper - fight the graveyard monsters and stop death
// by Pocket Pulp (@velipso), https://pulp.biz
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#include "books.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_ds.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

void books_help() {
  printf(
    "  books <sprites.png> <inputdir> <outputdir>\n"
    "    Generate books automatically\n"
  );
}

#define TPOS(x, y)   ((x) + (y) * 1024)
#define TPOS2(x, y)  ((x) + (y) * 1024 + (1 << 30))
#define TPOS3(x, y)  ((x) + (y) * 1024 + (2 << 30))

struct img_st {
  int w;
  int h;
  u32 *data;
};

enum row_type {
  ROW_TILES,
  ROW_TEXT,
  ROW_TEXTU,
  ROW_PARA,
  ROW_PAD,
  ROW_IMG
};

struct row_st {
  enum row_type type;
  union {
    struct {
      int size;
      int pos[10]; // x + y * 1024
    } tiles;
    char text[4000];
    struct {
      int lines;
      int width;
      char data[4000];
    } para;
    int pad;
    struct {
      int x;
      int y;
      int w;
      int h;
    } img;
  } u;
};

struct column_st {
  int x, y, w, h;
  struct row_st **rows;
};

struct book_st {
  u32 seed;
  int col;
  struct column_st column[2];
  char name[1000];
};

static const int SPACE_WIDTH = 4;
const u32 COLOR1 = 0xff1a1c22;
const u32 COLOR2 = 0xff63a4db;
static u32 g_book_seed = 123;
static struct book_st *book_current = NULL;
static struct {
  int x;
  int y;
  int w;
  char ch;
} chars[100];
static int chars_size = 0;
static int chars_data_width;
static int chars_data_height;
static u32 *chars_data = NULL;
static int bg_data_width;
static int bg_data_height;
static u32 *bg_data = NULL;
static int tiles_data_width;
static int tiles_data_height;
static u32 *tiles_data = NULL;
static int sprites_data_width;
static int sprites_data_height;
static u32 *sprites_data = NULL;

static uint32_t whisky2(uint32_t i0, uint32_t i1){
  uint32_t z0 = (i1 * 1833778363) ^ i0;
  uint32_t z1 = (z0 *  337170863) ^ (z0 >> 13) ^ z0;
  uint32_t z2 = (z1 *  620363059) ^ (z1 >> 10);
  uint32_t z3 = (z2 *  232140641) ^ (z2 >> 21);
  return z3;
}

static void load_chars_row(int row, const char *str) {
  int x = 1;
  for (int i = 0; str[i]; i++) {
    char ch = str[i];
    int w = 0;
    for (;; w++) {
      bool found = false;
      for (int y = 0; y < 9; y++) {
        u32 c = chars_data[x + w + (row * 9 + y) * chars_data_width];
        if (c != chars_data[0]) {
          found = true;
          break;
        }
      }
      if (!found) break;
    }
    int c = chars_size++;
    chars[c].x = x;
    chars[c].y = row * 9;
    chars[c].w = w;
    chars[c].ch = ch;
    x += w + 1;
  }
}

static int font_width(const char *text) {
  int w = 0;
  bool lastchar = false;
  for (int i = 0; text[i]; i++) {
    char ch = text[i];
    if (ch >= 1 && ch < 10) {
      lastchar = false;
      w += ch;
    } else if (ch == '\n') {
      lastchar = false;
    } else if (ch == ' ') {
      lastchar = false;
      w += SPACE_WIDTH;
    } else {
      for (int c = 0; c < chars_size; c++) {
        if (chars[c].ch == ch) {
          if (lastchar) w++;
          w += chars[c].w;
          lastchar = true;
          break;
        }
      }
    }
  }
  return w;
}

static void font_printch(struct img_st *img, int x, int y, int c) {
  for (int py = 0; py < 9; py++) {
    for (int px = 0; px < chars[c].w; px++) {
      u32 col = chars_data[(chars[c].x + px) + (chars[c].y + py) * chars_data_width];
      if (col != chars_data[0]) {
        img->data[(x + px) + (y + py) * img->w] = col;
      }
    }
  }
}

static void font_print(struct img_st *img, int x, int y, const char *text) {
  int dx = 0;
  bool lastchar = false;
  for (int i = 0; text[i]; i++) {
    char ch = text[i];
    if (ch >= 1 && ch < 10) {
      lastchar = false;
      dx += ch;
    } else if (ch == '\n') {
      lastchar = false;
      dx = 0;
      y += 9;
    } else if (ch == ' ') {
      lastchar = false;
      dx += SPACE_WIDTH;
    } else {
      for (int c = 0; c < chars_size; c++) {
        if (chars[c].ch == ch) {
          if (lastchar) dx++;
          font_printch(img, x + dx, y, c);
          dx += chars[c].w;
          lastchar = true;
          break;
        }
      }
    }
  }
}

static struct book_st *book_new(const char *outputdir, const char *name) {
  struct book_st *book = calloc(1, sizeof(struct book_st));
  book->seed = g_book_seed;
  g_book_seed = whisky2(g_book_seed, 123);
  snprintf(book->name, sizeof(book->name), "%s/scr_book_%s.png", outputdir, name);
  book->column[0].x = 16;
  book->column[0].y = 12;
  book->column[0].w = 95;
  book->column[0].h = 130;
  book->column[1].x = 128;
  book->column[1].y = 12;
  book->column[1].w = 95;
  book->column[1].h = 130;
  return book;
}

static void book_push(struct book_st *book, struct row_st *row) {
  arrpush(book->column[book->col].rows, row);
}

static void column_freebody(struct column_st *column) {
  for (int i = 0; i < arrlen(column->rows); i++) {
    free(column->rows[i]);
  }
  arrfree(column->rows);
}

static void book_free(struct book_st *book) {
  column_freebody(&book->column[0]);
  column_freebody(&book->column[1]);
  free(book);
}

static struct row_st *row_new(enum row_type type) {
  struct row_st *row = calloc(1, sizeof(struct row_st));
  row->type = type;
  return row;
}

static struct row_st *row_tiles1(int pos1) {
  struct row_st *row = row_new(ROW_TILES);
  row->u.tiles.size = 1;
  row->u.tiles.pos[0] = pos1;
  return row;
}

static struct row_st *row_tiles2(int pos1, int pos2) {
  struct row_st *row = row_new(ROW_TILES);
  row->u.tiles.size = 2;
  row->u.tiles.pos[0] = pos1;
  row->u.tiles.pos[1] = pos2;
  return row;
}

static struct row_st *row_tiles3(int pos1, int pos2, int pos3) {
  struct row_st *row = row_new(ROW_TILES);
  row->u.tiles.size = 3;
  row->u.tiles.pos[0] = pos1;
  row->u.tiles.pos[1] = pos2;
  row->u.tiles.pos[2] = pos3;
  return row;
}

static struct row_st *row_text(const char *text) {
  struct row_st *row = row_new(ROW_TEXT);
  snprintf(row->u.text, sizeof(row->u.text), "%s", text);
  return row;
}

static struct row_st *row_textu(const char *text) {
  struct row_st *row = row_new(ROW_TEXTU);
  snprintf(row->u.text, sizeof(row->u.text), "%s", text);
  return row;
}

static struct row_st *row_para(struct book_st *b, const char *text) {
  struct row_st *row = row_new(ROW_PARA);
  int max_width = b->column[b->col].w;
  int cur_width = 0;
  int line_width = 0;
  char *line = NULL;
  char **lines = NULL;
  char word[100];
  int wi = 0;
  int *justify = NULL;
  int *justify_width = NULL;
  for (int chi = 0; ; chi++) {
    char ch = text[chi];
    if (
      (ch >= 1 && ch < 10) ||
      ch == ' ' ||
      ch == '\n' ||
      ch == 0
    ) {
      word[wi++] = 0;
      // completed word
      int ww = font_width(word);
      int add_width = ww + (line_width > 0 ? SPACE_WIDTH : 0);
      if (line_width + add_width > max_width) {
        // need to wrap with this word
        arrpush(justify, arrlen(lines));
        arrpush(justify_width, line_width);
        arrpush(line, 0);
        arrpush(lines, line);
        line = NULL;
        line_width = ww;
      } else {
        // add this word to the line
        line_width += add_width;
      }
      if (line_width > cur_width) {
        cur_width = line_width;
      }
      if (arrlen(line) > 0) {
        arrpush(line, SPACE_WIDTH);
      }
      for (int i = 0; word[i]; i++) {
        arrpush(line, word[i]);
      }
      wi = 0;
      if (ch == '\n') {
        arrpush(line, 0);
        arrpush(lines, line);
        line = NULL;
        line_width = 0;
      } else if (ch == 0) {
        break;
      }
    } else {
      word[wi++] = ch;
    }
  }
  if (line) {
    arrpush(line, 0);
    arrpush(lines, line);
    line = NULL;
  }
  int pi = 0;
  int ji = 0;
  row->u.para.lines = arrlen(lines);
  row->u.para.width = cur_width;
  for (int i = 0; i < arrlen(lines); i++) {
    char *line = lines[i];
    bool jst = ji < arrlen(justify) && justify[ji] == i;
    if (jst) {
      int jst_left = cur_width - justify_width[ji];
      while (jst_left > 0) {
        bool changed = false;
        for (int x = 0; line[x]; x++) {
          if (line[x] >= 1 && line[x] < 9 && jst_left > 0) {
            changed = true;
            line[x]++;
            jst_left--;
          }
        }
        if (!changed) break; // nothing left to do so give up
      }
    }
    for (int j = 0; line[j]; j++) {
      row->u.para.data[pi++] = line[j];
    }
    row->u.para.data[pi++] = '\n';
    if (jst) {
      ji++;
    }
    arrfree(line);
  }
  row->u.para.data[pi] = 0;
  arrfree(lines);
  arrfree(justify);
  arrfree(justify_width);
  return row;
}

static struct row_st *row_pad(int pad) {
  struct row_st *row = row_new(ROW_PAD);
  row->u.pad = pad;
  return row;
}

static struct row_st *row_img(int x, int y, int w, int h) {
  struct row_st *row = row_new(ROW_IMG);
  row->u.img.x = x;
  row->u.img.y = y;
  row->u.img.w = w;
  row->u.img.h = h;
  return row;
}

static void img_free(struct img_st *img) {
  if (img->data) free(img->data);
  free(img);
}

static struct img_st *row_render(struct row_st *r, int width, u32 seed) {
  struct img_st *img = calloc(1, sizeof(struct img_st));
  switch (r->type) {
    case ROW_TILES: {
      img->w = 24 * r->u.tiles.size + 8; // 16 width + 8 collapsed margin
      img->h = 32;
      img->data = calloc(1, img->w * img->h * 4);
      for (int t = 0; t < r->u.tiles.size; t++) {
        int x = t * 24 + 8;
        int y = 8;
        int sx = 64 + 16 * (whisky2(seed, t) % 6);
        int sy = (r->u.tiles.pos[t] >> 30) * 16 + 64;
        // copy random tile background
        for (int py = 0; py < 16; py++) {
          for (int px = 0; px < 16; px++) {
            u32 c = tiles_data[(sx + px) + (sy + py) * tiles_data_width];
            if (c & 0xff000000) {
              img->data[(x + px) + (y + py) * img->w] = c;
            }
          }
        }
        // copy tile
        sx = r->u.tiles.pos[t] & 1023;
        sy = (r->u.tiles.pos[t] >> 10) & 1023;
        for (int py = 0; py < 16; py++) {
          for (int px = 0; px < 16; px++) {
            u32 c = tiles_data[(sx + px) + (sy + py) * tiles_data_width];
            if (c & 0xff000000) {
              img->data[(x + px) + (y + py) * img->w] = c;
            }
          }
        }
        // copy overlay
        int ox = -1, oy = -1;
        if (sx == 112 && sy == 32) { ox = 0; oy = 0; }
        else if (sx == 128 && sy == 32) { ox = 2; oy = 0; }
        else if (sx == 160 && sy == 112) { ox = 1; oy = 0; }
        else if (sx == 176 && sy == 112) { ox = 3; oy = 0; }
        else if (sx == 208 && sy == 80) { ox = 0; oy = 1; }
        else if (sx == 224 && sy == 80) { ox = 2; oy = 1; }
        else if (sx == 208 && sy == 112) { ox = 1; oy = 1; }
        else if (sx == 224 && sy == 112) { ox = 3; oy = 1; }
        if (ox >= 0) {
          ox *= 32;
          oy = (oy * 32) + 192;
          for (int py = 0; py < 32; py++) {
            for (int px = 0; px < 32; px++) {
              u32 c = sprites_data[(ox + px) + (oy + py) * sprites_data_width];
              if (c & 0xff000000) {
                img->data[(x + px - 8) + (y + py - 8) * img->w] = c;
              }
            }
          }
        }
      }
      break;
    }
    case ROW_TEXT:
    case ROW_TEXTU: {
      img->w = font_width(r->u.text);
      img->h = 9;
      img->data = calloc(1, img->w * img->h * 4);
      font_print(img, 0, 0, r->u.text);
      if (r->type == ROW_TEXTU) {
        // underline
        int y = 7;
        for (int x = 0; x < img->w; x++) {
          img->data[x + y * img->w] = COLOR1;
          int k = x + (y + 1) * img->w;
          if (img->data[k] != COLOR1) {
            img->data[k] = COLOR2;
          }
        }
      }
      break;
    }
    case ROW_PARA: {
      img->w = r->u.para.width;
      img->h = 9 * r->u.para.lines;
      img->data = calloc(1, img->w * img->h * 4);
      font_print(img, 0, 0, r->u.para.data);
      break;
    }
    case ROW_PAD: {
      img->w = 1;
      img->h = r->u.pad;
      img->data = NULL;
      break;
    }
    case ROW_IMG: {
      img->w = r->u.img.w;
      img->h = r->u.img.h;
      img->data = calloc(1, img->w * img->h * 4);
      for (int py = 0; py < r->u.img.h; py++) {
        for (int px = 0; px < r->u.img.w; px++) {
          u32 c = tiles_data[(r->u.img.x + px) + (r->u.img.y + py) * tiles_data_width];
          if (c & 0xff000000) {
            img->data[px + py * img->w] = c;
          }
        }
      }
      break;
    }
  }
  return img;
}

static void column_finish(u32 *img, struct column_st *c, u32 seed) {
  struct img_st **rows = NULL;
  int height = 0;
  for (int r = 0; r < arrlen(c->rows); r++) {
    struct img_st *ri = row_render(c->rows[r], c->w, whisky2(seed, r + 1));
    height += ri->h;
    arrpush(rows, ri);
  }

  int y = c->y + (c->h - height) / 2;
  for (int r = 0; r < arrlen(rows); r++) {
    struct img_st *ri = rows[r];
    if (ri->data) {
      // blit at center
      int x = c->x + (c->w - ri->w) / 2;
      for (int py = 0; py < ri->h; py++) {
        for (int px = 0; px < ri->w; px++) {
          u32 c = ri->data[px + py * ri->w];
          if ((c & 0xff000000) != 0) {
            img[(x + px) + (y + py) * bg_data_width] = c;
          }
        }
      }
    }
    y += ri->h;
    img_free(ri);
  }
  arrfree(rows);
}

static void bk_start(const char *outputdir, const char *name) {
  book_current = book_new(outputdir, name);
}
static void bk_nextcolumn() {
  book_current->col = 1;
}

static void bk_tiles1(int pos1) {
  book_push(book_current, row_tiles1(pos1));
  book_push(book_current, row_pad(-2));
}

static void bk_tiles2(int pos1, int pos2) {
  book_push(book_current, row_tiles2(pos1, pos2));
  book_push(book_current, row_pad(-2));
}

static void bk_tiles3(int pos1, int pos2, int pos3) {
  book_push(book_current, row_tiles3(pos1, pos2, pos3));
  book_push(book_current, row_pad(-2));
}

static void bk_text(const char *text) {
  book_push(book_current, row_text(text));
}

static void bk_textu(const char *text) {
  book_push(book_current, row_textu(text));
  book_push(book_current, row_pad(1));
}

static void bk_para(const char *text) {
  book_push(book_current, row_para(book_current, text));
}

static void bk_pad(int pad) {
  book_push(book_current, row_pad(pad));
}

static void bk_img(int x, int y, int w, int h) {
  book_push(book_current, row_img(x, y, w, h));
}

static void bk_finish() {
  struct book_st *b = book_current;
  printf("writing: %s\n", b->name);
  u32 *img = calloc(1, bg_data_width * bg_data_height * 4);
  memcpy(img, bg_data, bg_data_width * bg_data_height * 4);
  column_finish(img, &b->column[0], whisky2(b->seed, 1));
  column_finish(img, &b->column[1], whisky2(b->seed, 2));
  stbi_write_png(b->name, bg_data_width, bg_data_height, 4, img, bg_data_width * 4);
  free(img);
  book_free(b);
  book_current = NULL;
}

int books_main(int argc, const char **argv) {
  if (argc != 3) {
    books_help();
    fprintf(stderr, "\nExpecting: books <sprites.png> <inputdir> <outputdir>\n");
    return 1;
  }

  const char *inputdir = argv[1];
  const char *outputdir = argv[2];

  { // load font
    char file[1000];
    snprintf(file, sizeof(file), "%s/font.png", inputdir);
    FILE *fp = fopen(file, "rb");
    if (fp == NULL) {
      fprintf(stderr, "\nFailed to read: %s\n", file);
      return 1;
    }
    chars_data = (u32 *)stbi_load_from_file(fp, &chars_data_width, &chars_data_height, NULL, 4);
    fclose(fp);
    load_chars_row(0, "0123456789:!?.,'-+");
    load_chars_row(1, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    load_chars_row(2, "abcdefghijklmnopqrstuvwxyz");
  }

  { // load background
    char file[1000];
    snprintf(file, sizeof(file), "%s/bg.png", inputdir);
    FILE *fp = fopen(file, "rb");
    if (fp == NULL) {
      fprintf(stderr, "\nFailed to read: %s\n", file);
      return 1;
    }
    bg_data = (u32 *)stbi_load_from_file(fp, &bg_data_width, &bg_data_height, NULL, 4);
    fclose(fp);
  }

  { // load tiles
    char file[1000];
    snprintf(file, sizeof(file), "%s/tiles.png", inputdir);
    FILE *fp = fopen(file, "rb");
    if (fp == NULL) {
      fprintf(stderr, "\nFailed to read: %s\n", file);
      return 1;
    }
    tiles_data = (u32 *)stbi_load_from_file(fp, &tiles_data_width, &tiles_data_height, NULL, 4);
    fclose(fp);
  }

  { // load sprites
    char file[1000];
    snprintf(file, sizeof(file), "%s", argv[0]);
    FILE *fp = fopen(file, "rb");
    if (fp == NULL) {
      fprintf(stderr, "\nFailed to read: %s\n", file);
      return 1;
    }
    sprites_data =
      (u32 *)stbi_load_from_file(fp, &sprites_data_width, &sprites_data_height, NULL, 4);
    fclose(fp);
  }

  { // items
    bk_start(outputdir, "items1");
    bk_pad   (-8);
    bk_tiles1(TPOS2(0, 112));
    bk_pad   (-2);
    bk_textu ("HEART");
    bk_text  ("Heal to full HP");
    bk_tiles1(TPOS2(16, 112));
    bk_pad   (-2);
    bk_textu ("LANTERN");
    bk_text  ("Show area of the map");
    bk_tiles1(TPOS2(32, 112));
    bk_pad   (-2);
    bk_textu ("SPYDER GLASS");
    bk_text  ("Show all the spiders");
    bk_nextcolumn();
    bk_tiles1(TPOS2(48, 112));
    bk_textu ("EYE OF RA");
    bk_text  ("Show Scarabs and");
    bk_text  ("Anubis Warriors");
    bk_tiles3(TPOS2(64, 112), TPOS2(80, 112), TPOS2(96, 112));
    bk_textu ("EXPERIENCE");
    bk_text  ("Add experience points");
    bk_finish();

    bk_start(outputdir, "items2");
    bk_tiles1(TPOS2(144, 112));
    bk_textu ("DRACULA'S IRE");
    bk_text  ("? ? ?");
    bk_tiles1(TPOS2(0, 96));
    bk_textu ("EXIT DOOR");
    bk_text  ("Escape after killing");
    bk_text  ("Death");
    bk_nextcolumn();
    bk_img   (0, 14, 44, 18);
    bk_pad   (8);
    bk_textu ("BOOKS");
    bk_pad   (3);
    bk_para(
      "Unlock books by beating the game in different ways.\n\n"
      "Try skipping EXP drops or leaving items behind!"
    );
    bk_finish();
  }

  { // lowlevel
    bk_start(outputdir, "lowlevel");
    bk_tiles2(TPOS(112, 16), TPOS(128, 16));
    bk_textu ("GRAVE SPIDER");
    bk_text  ("Level: 1");
    bk_pad   (3);
    bk_tiles2(TPOS(112, 48), TPOS(128, 48));
    bk_textu ("GHOST KID");
    bk_text  ("Level: 2");
    bk_nextcolumn();
    bk_para(
      "The Grave Spider is randomly placed. Squash it before its creepy legs find you first!\n\n"
      "The Ghost Kid pops up randomly as well, giggling at his own bad jokes. Remind him he's dead!"
    );
    bk_finish();
  }

  { // lv1b
    bk_start(outputdir, "lv1b");
    bk_tiles2(TPOS(112, 32), TPOS(128, 32));
    bk_textu ("MUMMY");
    bk_text  ("Level: 1");
    bk_nextcolumn();
    bk_para(
      "Placed on the edge of the screen, this shy mummy awkwardly shuffles around.\n\n"
      "Defeat it to expose its hidden guardians - the sneaky Scarabs and fierce Anubis Warriors!"
    );
    bk_finish();
  }

  { // lv3a
    bk_start(outputdir, "lv3a");
    bk_tiles2(TPOS(160, 0), TPOS(176, 0));
    bk_textu ("BANSHEE SISTER");
    bk_text  ("Level: 3");
    bk_nextcolumn();
    bk_para(
      "Randomly placed and eager to practice her piercing screams on you.\n\n"
      "She's already loud enough solo, don't wait around for the rest of her spectral family "
      "reunion!"
    );
    bk_finish();
  }

  { // lv3bc
    bk_start(outputdir, "lv3bc");
    bk_tiles2(TPOS(160, 16), TPOS(176, 16));
    bk_textu ("BANSHEE TWINS");
    bk_text  ("Level: 3");
    bk_pad   (3);
    bk_tiles3(TPOS(160, 32), TPOS(176, 32), TPOS(160, 32));
    bk_textu ("BANSHEE TRIPLETS");
    bk_text  ("Level: 3");
    bk_nextcolumn();
    bk_para(
      "Twins: Randomly placed duo - double the shrieks, double the trouble! Defeat both twins to "
      "claim your 6 EXP reward.\n\n"
      "Triplets: Triple threat haunting random spots. Silence all three before earning your 9 EXP!"
    );
    bk_finish();
  }

  { // lv4a
    bk_start(outputdir, "lv4a");
    bk_tiles2(TPOS(160, 48), TPOS(176, 48));
    bk_textu ("SKELETON");
    bk_text  ("Level: 4");
    bk_nextcolumn();
    bk_para(
      "Placed in pairs horizontally or vertically - these bony buddies always stick together, "
      "straight up plotting your demise."
    );
    bk_finish();
  }

  { // lv4b
    bk_start(outputdir, "lv4b");
    bk_tiles2(TPOS(160, 64), TPOS(176, 64));
    bk_textu ("DISCO SKELETON");
    bk_text  ("Level: 4");
    bk_nextcolumn();
    bk_para(
      "Appearing in pairs diagonally, they're ready to boogie into your grave.\n\n"
      "Break their rhythm before they break yours!"
    );
    bk_finish();
  }

  { // lv4c
    bk_start(outputdir, "lv4c");
    bk_tiles2(TPOS(160, 80), TPOS(176, 80));
    bk_textu ("SKELETON KNIGHT");
    bk_text  ("Level: 4");
    bk_nextcolumn();
    bk_para(
      "Always placed in pairs a knight's move apart, these noble boneheads joust around "
      "awkwardly.\n\n"
      "Knock 'em off their high horses!"
    );
    bk_finish();
  }

  { // lv5a
    bk_start(outputdir, "lv5a");
    bk_tiles2(TPOS(160, 96), TPOS(176, 96));
    bk_textu ("SCARAB");
    bk_text  ("Level: 5");
    bk_nextcolumn();
    bk_para(
      "Scuttles randomly beneath the board, waiting to be uncovered by a defeated mummy.\n\n"
      "Squish it before it bugs you!"
    );
    bk_finish();
  }

  { // lv5b
    bk_start(outputdir, "lv5b");
    bk_tiles2(TPOS(160, 112), TPOS(176, 112));
    bk_textu ("WIDOW QUEEN");
    bk_text  ("Level: 5");
    bk_nextcolumn();
    bk_para(
      "Crawling randomly around the graveyard, this protective arachnid mom is hiding her "
      "mischievous spiderlings.\n\n"
      "Squish her, and she'll reveal every creepy little Grave Spider lurking nearby!"
    );
    bk_finish();
  }

  { // lv5c
    bk_start(outputdir, "lv5c");
    bk_tiles2(TPOS(208, 0), TPOS(224, 0));
    bk_textu ("SWAMP WITCH");
    bk_text  ("Level: 5");
    bk_nextcolumn();
    bk_para(
      "Randomly placed, this sneaky hag conceals enemy counts in a diamond around her.\n\n"
      "Defeat her to clear the fog of war - and her terrible cooking!"
    );
    bk_finish();
  }

  { // lv6
    bk_start(outputdir, "lv6");
    bk_tiles2(TPOS(208, 16), TPOS(224, 16));
    bk_textu ("DRAUGR");
    bk_text  ("Level: 6");
    bk_nextcolumn();
    bk_para(
      "Always lurking near treasure chests, this undead hoarder guards his loot fiercely.\n\n"
      "Defeat him to claim what's rightfully yours!"
    );
    bk_finish();
  }

  { // lv7
    bk_start(outputdir, "lv7");
    bk_tiles2(TPOS(208, 32), TPOS(224, 32));
    bk_textu ("ZOMBOX");
    bk_text  ("Level: 7");
    bk_nextcolumn();
    bk_para(
      "Four zombies perfectly guarding the corners of their territory in synchronized undead "
      "teamwork.\n\n"
      "Break their formation, ruin their routine, and send them stumbling!"
    );
    bk_finish();
  }

  { // lv8
    bk_start(outputdir, "lv8");
    bk_tiles2(TPOS(208, 48), TPOS(224, 48));
    bk_textu ("ANUBIS WARRIOR");
    bk_text  ("Level: 8");
    bk_nextcolumn();
    bk_para(
      "Always placed protectively around their mummy overlord, these overly-serious guardians "
      "patrol tirelessly.\n\n"
      "Send them packing back to the underworld - someone needs to lighten up that afterlife!"
    );
    bk_finish();
  }

  { // lv9
    bk_start(outputdir, "lv9");
    bk_tiles2(TPOS(208, 64), TPOS(224, 64));
    bk_textu ("PHANTOM TWINS");
    bk_text  ("Level: 9");
    bk_nextcolumn();
    bk_para(
      "Eerily placed as mirrored twins on the same row, these creepy siblings share everything - "
      "including a surprisingly generous habit of dropping hearts when defeated.\n\n"
      "Smash their spooky symmetry!"
    );
    bk_finish();
  }

  { // lv10
    bk_start(outputdir, "lv10");
    bk_tiles2(TPOS(208, 80), TPOS(224, 80));
    bk_textu ("DRACULA");
    bk_text  ("Level: 10");
    bk_nextcolumn();
    bk_pad   (10);
    bk_para(
      "Hidden randomly in a dark corner, Dracula broods in silence, annoyed by the noise and "
      "bright lights of modern graveyards.\n\n"
      "Defeat him to unleash Dracula's Ire - a tantrum so powerful it consumes all mines and "
      "floods the map with bubbling lava.\n\n"
      "He may be undead, but he's still dramatic!"
    );
    bk_finish();
  }

  { // lv11
    bk_start(outputdir, "lv11");
    bk_tiles2(TPOS(208, 96), TPOS(224, 96));
    bk_textu ("MIMIC");
    bk_text  ("Level: 11");
    bk_nextcolumn();
    bk_para(
      "Disguised as a chest - but beware!\n\n"
      "Appearing only on Normal, Hard, and Expert difficulties, its suspiciously high level gives "
      "away this treasure trap."
    );
    bk_finish();
  }

  { // lv13
    bk_start(outputdir, "lv13");
    bk_tiles2(TPOS(208, 112), TPOS(224, 112));
    bk_textu ("DEATH");
    bk_text  ("Level: 13");
    bk_nextcolumn();
    bk_para(
      "He's tired, cranky, and determined to retire - by ending all humanity!\n\n"
      "Defeat him to foil his dark retirement plans and claim victory!"
    );
    bk_finish();
  }

  { // mine
    bk_start(outputdir, "mine");
    bk_tiles1(TPOS(64, 16));
    bk_textu ("MINE");
    bk_text  ("Instant Death");
    bk_nextcolumn();
    bk_para(
      "Hidden randomly beneath the graveyard, stepping on this deadly surprise means instant "
      "death!\n\n"
      "But hold L+R when selecting your difficulty, and you'll uncover a secret, transforming your "
      "spooky adventure into a classic challenge!"
    );
    bk_finish();
  }

  { // wall
    bk_start(outputdir, "wall");
    bk_tiles1(TPOS(16, 96));
    bk_text  ("WALL");
    bk_nextcolumn();
    bk_para(
      "Blocks your vision - but you can smash through if you're feeling bold!\n\n"
      "It costs whatever you can afford:\n\n"
      "Costs 6 HP, drops 5 EXP\n"
      "Costs 4 HP, drops 3 EXP\n"
      "Costs 2 HP, drops 1 EXP\n"
      "Costs 1 HP, drops nothing!"
    );
    bk_finish();
  }

  { // chest
    bk_start(outputdir, "chest");
    bk_tiles1(TPOS(208, 96));
    bk_text  ("CHEST");
    bk_nextcolumn();
    bk_para(
      "Randomly scattered around the graveyard.\n\n"
      "One lucky chest holds a precious lantern, three others hide 5 EXP each, and the rest "
      "generously offer hearts.\n\n"
      "Open carefully - treasure hunting has never felt so spooky!"
    );
    bk_finish();
  }

  { // lava
    bk_start(outputdir, "lava");
    bk_tiles1(TPOS2(144, 112));
    bk_textu ("DRACULA'S IRE");
    bk_text  ("Activate lava to");
    bk_text  ("explode all mines");
    bk_tiles1(TPOS(64, 0));
    bk_textu ("LAVA");
    bk_text  ("-1 HP");
    bk_nextcolumn();
    bk_para(
      "Once you collect Dracula's Ire, lava floods the board, consuming all mines.\n\n"
      "It leaves behind bubbling pools with enemy counts - but don't hover over them with your "
      "cursor, or you'll take 1 HP of sizzling regret!"
    );
    bk_finish();
  }

  #if 0
  {
    bk_start(outputdir, "how2");
    bk_nextcolumn();
    bk_para(
      "Zero hearts is okay - just don't drop below!\n\n"
      "Defeat Death to win.\n\n"
      "Kill monsters, collect EXP, and level up to stay alive!\n\n"
      "Enemies follow patterns - keep playing to learn them!"
    );
    bk_finish();
  }
  #endif

  #if 0
  {
    bk_start(outputdir, "credits");
    bk_nextcolumn();
    bk_para(
      "CREDITS\n"
      "Crypt Sweeper made by Sean Connelly and Casey Dean\n"
      "c 2025 Pocket Pulp LLC\n"
      "All Rights Reserved\n"
      "www.pulp.biz\n"
      "SPECIAL THANKS\n"
      "danielben - Creator of DragonSweeper which inspired this game\n"
      "L.Beethoven - Moonlight Sonata\n"
      "Adigun Polack - AAP-64 Palette"
    );
    bk_finish();
  }
  #endif

  stbi_image_free(sprites_data);
  stbi_image_free(tiles_data);
  stbi_image_free(bg_data);
  stbi_image_free(chars_data);
  return 0;
}

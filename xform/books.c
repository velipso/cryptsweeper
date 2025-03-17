//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
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
  ROW_PAD
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
  book->column[0].y = 8;
  book->column[0].w = 95;
  book->column[0].h = 130;
  book->column[1].x = 128;
  book->column[1].y = 8;
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

static struct row_st *row_tiles4(int pos1, int pos2, int pos3, int pos4) {
  struct row_st *row = row_new(ROW_TILES);
  row->u.tiles.size = 4;
  row->u.tiles.pos[0] = pos1;
  row->u.tiles.pos[1] = pos2;
  row->u.tiles.pos[2] = pos3;
  row->u.tiles.pos[3] = pos4;
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

static void img_free(struct img_st *img) {
  if (img->data) free(img->data);
  free(img);
}

static struct img_st *row_render(struct row_st *r, int width, u32 seed) {
  struct img_st *img = calloc(1, sizeof(struct img_st));
  switch (r->type) {
    case ROW_TILES: {
      img->w = 24 * r->u.tiles.size - 8; // 16 width + 8 margin right
      img->h = 32;
      img->data = calloc(1, img->w * img->h * 4);
      for (int t = 0; t < r->u.tiles.size; t++) {
        int x = t * 24;
        int y = 8;
        int sx = 64 + 16 * (whisky2(seed, t) % 6);
        int sy = 64;
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
        sy = r->u.tiles.pos[t] >> 10;
        for (int py = 0; py < 16; py++) {
          for (int px = 0; px < 16; px++) {
            u32 c = tiles_data[(sx + px) + (sy + py) * tiles_data_width];
            if (c & 0xff000000) {
              img->data[(x + px) + (y + py) * img->w] = c;
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

static void bk_tiles4(int pos1, int pos2, int pos3, int pos4) {
  book_push(book_current, row_tiles4(pos1, pos2, pos3, pos4));
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
    load_chars_row(0, "0123456789:!?.,'");
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

  { // lowlevel
    bk_start(outputdir, "lowlevel");
    bk_tiles2(TPOS(112, 16), TPOS(128, 16)); // grave spider
    bk_textu ("GRAVE SPIDER");
    bk_text  ("Level: 1");
    bk_pad   (3);
    bk_tiles2(TPOS(112, 48), TPOS(128, 48)); // ghost kid
    bk_textu ("GHOST KID");
    bk_text  ("Level: 2");
    bk_nextcolumn();
    bk_para(
      "The Grave Spider is randomly placed. Squash it before its creepy legs find you first!\n\n"
      "The Ghost Kid pops up randomly as well, giggling at his own bad jokes. Remind him he's dead!"
    );
    bk_finish();
  }

  stbi_image_free(sprites_data);
  stbi_image_free(tiles_data);
  stbi_image_free(bg_data);
  stbi_image_free(chars_data);
  return 0;
}

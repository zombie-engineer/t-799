#pragma once
#include <stdint.h>

struct gfx_font;

struct canvas_pos {
  uint16_t x;
  uint16_t y;
};

struct canvas_size {
  uint16_t x;
  uint16_t y;
};

struct canvas_area {
  struct canvas_pos pos;
  struct canvas_size size;
};

struct canvas_plot {
  struct canvas_area area;
  struct canvas_pos last_pos;
  uint64_t max_value;
};

struct canvas_plot_with_value_text {
  struct canvas_area text_area;
  struct canvas_plot plot;
  const uint32_t *value;
  const char *fmt_suffix;
  uint32_t value_div_kilo;
};

struct canvas {
  struct canvas_size size;
  uint16_t stride;

  /* pixels */
  uint8_t *data;

  /* foreground color */
  uint8_t fg_r;
  uint8_t fg_g;
  uint8_t fg_b;

  /* background color */
  uint8_t bg_r;
  uint8_t bg_g;
  uint8_t bg_b;
};

#define CANVAS_PLOT_WITH_TEXT_INIT(__prefix, __p_value, __fmt_suffix, \
  __value_div) \
  { \
    .text_area = { \
      .pos = { \
        .x = __prefix ## _X, \
        .y = __prefix ## _Y \
      }, \
      .size = { \
        .x = __prefix ## _TEXT_SIZE_X, \
        .y = __prefix ## _SIZE_Y \
      }, \
    }, \
    .plot = { \
      .area = { \
        .pos = { \
          .x = __prefix ## _TEXT_SIZE_X, \
          .y = __prefix ## _Y \
        }, \
        .size = { \
          .x = __prefix ## _X + __prefix ## _TEXT_SIZE_X, \
          .y = __prefix ## _SIZE_Y \
        }, \
      }, \
      .max_value = __prefix ## _MAX_VALUE \
    }, \
    .value = __p_value, \
    .fmt_suffix = __fmt_suffix, \
    .value_div_kilo = __value_div \
  }

void canvas_init(struct canvas *c, uint8_t *data, struct canvas_size *size);

void canvas_draw_line(struct canvas *c, unsigned x0, unsigned y0, unsigned x1,
  unsigned y1, uint8_t r, uint8_t g, uint8_t b);

void canvas_fill_rect(struct canvas *c, int x, int y, int w, int h, uint8_t r,
  uint8_t g, uint8_t b);

void canvas_draw_text(struct canvas *c, int x, int y,
  const struct gfx_font *font, const char *text, uint8_t r, uint8_t g,
  uint8_t b);

void canvas_plot_with_value_text_draw(struct canvas *c,
  struct canvas_plot_with_value_text *p, uint32_t value);

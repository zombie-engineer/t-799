#pragma once
#include <stdint.h>

struct gfx_font;

struct diagram {
  uint16_t x;
  uint16_t y;
  uint16_t size_x;
  uint16_t size_y;
  uint64_t max_value;
  uint16_t last_x;
  uint16_t last_y;
};

struct canvas {
  uint16_t size_x;
  uint16_t size_y;
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

void canvas_init(struct canvas *c, uint8_t *data, uint16_t size_x,
  uint16_t size_y);

void canvas_draw_line(struct canvas *c, unsigned x0, unsigned y0, unsigned x1,
  unsigned y1, uint8_t r, uint8_t g, uint8_t b);

void canvas_fill_rect(struct canvas *c, int x, int y, int w, int h, uint8_t r,
  uint8_t g, uint8_t b);

void canvas_draw_text(struct canvas *c, int x, int y,
  const struct gfx_font *font, const char *text, uint8_t r, uint8_t g,
  uint8_t b);

void canvas_diagram_draw_one(struct canvas *c, struct diagram *d,
  uint32_t value);

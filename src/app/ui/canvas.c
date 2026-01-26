#include "canvas.h"
#include "font/gfx_font.h"
#include <printf.h>

static inline void canvas_set_pixel_color(struct canvas *c,
    unsigned x, unsigned y,
    uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t *p;
  if (x >= c->size_x|| y >= c->size_y)
    return;

  p = c->data + (y * c->size_x + x) * 3;
  p[0] = r;
  p[1] = g;
  p[2] = b;
}

void canvas_diagram_draw_one(struct canvas *c, struct diagram *d,
  uint32_t value)
{
  d->last_x++;
  if (d->last_x > d->size_x) {
    d->last_x = 0;
  }

  unsigned int x = d->last_x + d->x;

  uint32_t value_scaled = ((float)value / d->max_value) * d->size_y;
  if (value_scaled > d->size_y) {
    value_scaled = d->size_y;
  }

  unsigned int y = d->y + d->size_y - value_scaled;
  printf("value: %d, scaled: %d, max:%d, y:%d\r\n", value,  value_scaled,
    d->max_value, y);

  if (x == d->x)
    canvas_set_pixel_color(c, d->x, y, 255, 0, 0);
  else {
    canvas_draw_line(c, x, d->y, x, d->y + d->size_y, 0, 0, 0);
    canvas_draw_line(c, x - 1, d->last_y, x, y, 255, 0, 0);
  }
  d->last_y = y;
}

void canvas_fill_rect(struct canvas *c, int x, int y, int w, int h, uint8_t r,
  uint8_t g, uint8_t b)
{
  uint8_t *row;
  uint8_t *p;

  if (!c || !c->data || w <= 0 || h <= 0)
    return;

  if (x < 0) {
    w += x;
    x = 0;
  }

  if (y < 0) {
    h += y;
    y = 0;
  }

  if (x + w > (int)c->size_x)
    w = c->size_x - x;

  if (y + h > (int)c->size_y)
    h = c->size_y - y;

  if (w <= 0 || h <= 0)
    return;

  row = c->data + y * c->stride + x * 3;

  for (int yy = 0; yy < h; yy++) {
    p = row;
    for (int xx = 0; xx < w; xx++) {
      *p++ = r;
      *p++ = g;
      *p++ = b;
    }
    row += c->stride;
  }
}

void canvas_draw_glyph(struct canvas *c, int x, int y,
  const struct gfx_font *font, uint8_t ch, uint8_t r, uint8_t g, uint8_t b)
{
  if (!font)
    return;

  if (ch < font->first || ch > font->last)
    return;

  const struct gfx_glyph *glyph = &font->glyph[ch - font->first];
  const uint8_t *bitmap = font->bitmap + glyph->bitmap_offset;

  int bw = glyph->width;
  int bh = glyph->height;

  int xo = glyph->x_offset;
  int yo = glyph->y_offset;

  int bit = 0;
  uint8_t bits = 0;

  for (int yy = 0; yy < bh; yy++) {
    for (int xx = 0; xx < bw; xx++) {

      if ((bit & 7) == 0) {
        bits = *bitmap++;
      }

      if (bits & 0x80) {
        canvas_set_pixel_color(c, x + xo + xx, y + yo + yy, r, g, b);
      }

      bits <<= 1;
      bit++;
    }
  }
}

void canvas_draw_line(struct canvas *c, unsigned x0, unsigned y0, unsigned x1,
  unsigned y1, uint8_t r, uint8_t g, uint8_t b)
{
  int dx = (int)x1 - (int)x0;
  int dy = (int)y1 - (int)y0;

  int sx = (dx >= 0) ? 1 : -1;
  int sy = (dy >= 0) ? 1 : -1;

  dx = dx >= 0 ? dx : -dx;
  dy = dy >= 0 ? dy : -dy;

  int err = (dx > dy ? dx : -dy) / 2;
  int e2;

  for (;;) {
    canvas_set_pixel_color(c, x0, y0, r, g, b);

    if (x0 == x1 && y0 == y1)
      break;

    e2 = err;

    if (e2 > -dx) {
      err -= dy;
      x0 += sx;
    }

    if (e2 < dy) {
      err += dx;
      y0 += sy;
    }
  }
}

void canvas_draw_text(struct canvas *c, int x, int y,
  const struct gfx_font *font, const char *text, uint8_t r, uint8_t g,
  uint8_t b)
{
  const struct gfx_glyph *glyph;

  if (!font || !text)
    return;

  int cursor_x = x;
  int cursor_y = y;

  while (*text) {
    char ch = *text++;

    if (ch == '\n') {
      cursor_x = x;
      cursor_y += font->y_advance;
      continue;
    }

    if (ch < font->first || ch > font->last)
      continue;

    glyph = &font->glyph[ch - font->first];

    canvas_draw_glyph(c, cursor_x, cursor_y, font, ch, r, g, b);
    cursor_x += glyph->x_advance;
  }
}

void canvas_init(struct canvas *c, uint8_t *data, uint16_t size_x,
  uint16_t size_y)
{
  c->size_x = size_x;
  c->size_y = size_y;
  c->data = data;
  c->stride = 3 * size_x;
}

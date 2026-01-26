#include "ui.h"
#include "font/font_free_sans8pt7b.h"
#include "font/font_free_sans6pt7b.h"
#include "canvas.h"
#include <stringlib.h>

static const struct ui_conf *ui_conf = NULL;
static struct diagram diagram_bps;
static struct diagram diagram_fps;
static struct canvas ui_canvas;

static void ui_draw_fps(uint32_t fps)
{
  char buf[64];
  uint32_t value;
  const struct gfx_font *f = &font_free_sans_6pt_7b;
  int x = ui_conf->diagram_fps.text_pos_x;
  int y = ui_conf->diagram_fps.text_pos_y;
  int size_x = ui_conf->diagram_fps.text_size_x;
  int size_y = ui_conf->diagram_fps.graph_size_y;

  value = fps * 100;
  snprintf(buf, sizeof(buf), "%d.%02dfps", value / 100, value % 100);
  canvas_fill_rect(&ui_canvas, x, y, size_x, size_y, 0, 0, 0);
  canvas_draw_text(&ui_canvas, x, y + f->y_advance / 2, f, buf, 255, 0, 0);
  canvas_diagram_draw_one(&ui_canvas, &diagram_fps, fps);
}

static void ui_draw_bitrate(uint32_t bitrate)
{
  char buf[64];
  uint32_t value;
  const struct gfx_font *f = &font_free_sans_6pt_7b;
  int x = ui_conf->diagram_bps.text_pos_x;
  int y = ui_conf->diagram_bps.text_pos_y;
  int size_x = ui_conf->diagram_fps.text_size_x;
  int size_y = ui_conf->diagram_fps.graph_size_y;

  if (bitrate > 1000000) {
    value = bitrate * 100 / 1000000;
    snprintf(buf, sizeof(buf), "%d.%02dMbps", value / 100, value % 100);
  } else if (bitrate > 1000) {
    value = bitrate * 100 / 1000;
    snprintf(buf, sizeof(buf), "%d.%02dKbps", value / 100, value % 100);
  } else {
    snprintf(buf, sizeof(buf), "%dbps", bitrate);
  }
  canvas_fill_rect(&ui_canvas, x, y, size_x, size_y, 0, 0, 0);
  canvas_draw_text(&ui_canvas, x, y + f->y_advance / 2, f, buf, 255, 0, 0);
  canvas_diagram_draw_one(&ui_canvas, &diagram_bps, bitrate);
}

static inline void diagram_init(struct diagram *d,
  const struct ui_diagram_conf *conf)
{
  d->x = conf->graph_pos_x;
  d->y = conf->graph_pos_y;
  d->size_x = conf->graph_size_x;
  d->size_y = conf->graph_size_y;
  d->max_value = conf->max_value;
  d->last_y = d->y + d->size_y;
}

void ui_redraw(const struct ui_data *d)
{
  unsigned x0, x1, y;
  const struct ui_diagram_conf *const conf = &ui_conf->diagram_bps;

  x0 = 2;
  y = conf->text_pos_y + conf->text_size_y;
  x1 = conf->text_size_x + conf->graph_size_x - x0;

  canvas_draw_line(&ui_canvas, x0, y, x1, y, 0, 255, 0);

  ui_draw_bitrate(d->bitrate);
  ui_draw_fps(d->fps);
}

void ui_init(const struct ui_conf * const conf, uint8_t *data,
  size_t data_size)
{
  ui_conf = conf;
  diagram_init(&diagram_bps, &conf->diagram_bps);
  diagram_init(&diagram_fps, &conf->diagram_fps);
  canvas_init(&ui_canvas, data, ui_conf->canvas_size_x,
    ui_conf->canvas_size_y);
}

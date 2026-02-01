#include "ui.h"
#include "font/font_free_sans8pt7b.h"
#include "font/font_free_sans6pt7b.h"
#include "canvas.h"
#include <stringlib.h>
#include <printf.h>

static struct ui *_ui = NULL;
static struct canvas ui_canvas;

/* gmkb - gyga, mega, kilo, byte */
static inline void ui_scaled_value_to_string(char *buf, size_t buf_size,
  uint32_t v, const char *unit, uint32_t value_div_kilo)
{
  uint32_t val_scaled;
  uint32_t value_div_mega = value_div_kilo * value_div_kilo;

  if (v > value_div_mega) {
    val_scaled = v * 100 / value_div_mega;
    snprintf(buf, buf_size, "%d.%02dM%s", val_scaled / 100,
      val_scaled % 100, unit);
  } else if (v > value_div_kilo) {
    val_scaled = v * 100 / 1000;
    snprintf(buf, buf_size, "%d.%02dK%s", val_scaled / 100,
      val_scaled % 100, unit);
  } else {
    snprintf(buf, buf_size, "%d%s", v, unit);
  }
}

static void ui_plot_redraw(struct canvas_plot_with_value_text *p)
{
  char buf[64];
  const struct gfx_font *f = &font_free_sans_6pt_7b;
  int x = p->text_area.pos.x;
  int y = p->text_area.pos.y;
  int size_x = p->text_area.size.x;
  int size_y = p->plot.area.size.y;
  uint32_t value = *(uint32_t *)p->value;

  ui_scaled_value_to_string(buf, sizeof(buf), value, p->fmt_suffix,
    p->value_div_kilo);
  canvas_fill_rect(&ui_canvas, x, y, size_x, size_y, 0, 0, 0);
  canvas_draw_text(&ui_canvas, x, y + f->y_advance / 2, f, buf, 255, 0, 0);
  canvas_plot_with_value_text_draw(&ui_canvas, p, value);
}

static void ui_redraw_num_bufs(uint32_t nr_bufs_on_arm,
  uint32_t nr_bufs_from_vc, uint32_t nr_bufs_to_vc)
{
  char buf[64];
  const struct gfx_font *f = &font_free_sans_6pt_7b;
  int x = _ui->h264_num_bufs.pos.x;
  int y = _ui->h264_num_bufs.pos.y;
  int size_x = _ui->h264_num_bufs.size.y;
  int size_y = _ui->h264_num_bufs.size.y;

  snprintf(buf, sizeof(buf), "%d/%d/%d", nr_bufs_on_arm, nr_bufs_from_vc,
    nr_bufs_to_vc);

  canvas_fill_rect(&ui_canvas, x, y, size_x + 60, size_y, 0, 0, 0);
  canvas_draw_text(&ui_canvas, x, y + f->y_advance / 2, f, buf, 255, 0, 0);
}

static inline void plot_init(struct canvas_plot_with_value_text *p)
{
  p->plot.last_pos.x = p->plot.area.size.x - 1;
  p->plot.last_pos.y = p->plot.area.size.y;
}

void ui_redraw(const struct ui_data *d)
{
  for (size_t i = 0 ; i < _ui->num_plots; ++i)
    ui_plot_redraw(&_ui->plots[i]);

  ui_redraw_num_bufs(d->h264_bufs_on_arm, d->h264_bufs_from_vc,
    d->h264_bufs_to_vc);
}

void ui_init(struct ui *ui, uint8_t *data, size_t data_size)
{
  _ui = ui;
  for (size_t i = 0 ; i < _ui->num_plots; ++i)
    plot_init(&_ui->plots[i]);
  canvas_init(&ui_canvas, data, &_ui->canvas_size);
}

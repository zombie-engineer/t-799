#pragma once
#include <stdint.h>
#include <stddef.h>
#include "canvas.h"

struct ui_data {
  uint32_t num_h264_buffers;
  uint32_t h264_bps;
  uint32_t h264_fps;
  uint32_t sd_write_bps;
};

struct ui {
  struct canvas_size canvas_size;
  struct canvas_plot_with_value_text *plots;
  size_t num_plots;
};

void ui_init(struct ui *ui, uint8_t *data, size_t data_size);

void ui_redraw(const struct ui_data *d);

#pragma once
#include <stdint.h>
#include <stddef.h>
#include "canvas.h"

struct ui_data {
  uint32_t h264_bufs_on_arm;
  uint32_t h264_bufs_from_vc;
  uint32_t h264_bufs_to_vc;
  uint32_t h264_bps;
  uint32_t h264_fps;
  uint32_t sd_write_bps;
};

struct ui {
  struct canvas_size canvas_size;
  struct canvas_plot_with_value_text *plots;
  size_t num_plots;
  struct canvas_area h264_num_bufs;
};

void ui_init(struct ui *ui, uint8_t *data, size_t data_size);

void ui_redraw(const struct ui_data *d);

#pragma once
#include <stdint.h>
#include <stddef.h>

struct ui_data {
  uint32_t num_h264_buffers;
  uint32_t bitrate;
  uint32_t fps;
  uint32_t bps_sd_writes;
};

struct ui_diagram_conf {
  int text_pos_x;
  int text_pos_y;
  int text_size_x;
  int text_size_y;
  int graph_pos_x;
  int graph_pos_y;
  int graph_size_x;
  int graph_size_y;
  uint64_t max_value;
};

struct ui_conf {
  struct ui_diagram_conf diagram_bps;
  struct ui_diagram_conf diagram_fps;
  struct ui_diagram_conf diagram_bps_sd_writes;
  uint16_t canvas_size_x;
  uint16_t canvas_size_y;
};

void ui_init(const struct ui_conf *const ui_conf, uint8_t *data,
  size_t data_size);

void ui_redraw(const struct ui_data *d);

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <vc/service_mmal_param.h>
#include <drivers/display/display_ili9341.h>

struct block_device;

struct camera_video_port_config_h264 {
  uint32_t q_initial;
  uint32_t q_min;
  uint32_t q_max;
  uint32_t intraperiod;
  uint32_t bitrate;
  enum mmal_video_profile profile;
  enum mmal_video_level level;
};

struct camera_config {
  struct block_device *block_device;
  unsigned int frame_size_x;
  unsigned int frame_size_y;
  unsigned int frames_per_sec;
  unsigned int bit_rate;
  bool enable_preview;
  struct ili9341_drawframe *preview_drawframe;
  unsigned int preview_size_x;
  unsigned int preview_size_y;
  size_t h264_buffers_num;
  size_t h264_buffer_size;
  struct camera_video_port_config_h264 h264_encoder;
};

int camera_init(const struct camera_config *conf);

int camera_video_start(void);

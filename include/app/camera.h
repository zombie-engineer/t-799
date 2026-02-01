#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <vc/service_mmal_param.h>
#include <drivers/display/display_ili9341.h>

#define CAM_PORT_STATS_H264 1

struct block_device;

struct cam_port_stats {
  uint32_t bufs_from_vc;
  uint32_t bufs_to_vc;
  uint32_t configs_from_vc;
  uint32_t keyframes_from_vc;
  uint32_t frame_start_from_vc;
  uint32_t frame_end_from_vc;
  uint32_t nal_end_from_vc;
  uint32_t bytes_from_vc;
  uint16_t bufs_on_vc;
  uint16_t bufs_on_arm;
};

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
void cam_port_stats_fetch(struct cam_port_stats *dst, int stats_id);

int camera_video_start(void);

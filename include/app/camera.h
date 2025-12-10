#pragma once
#include <stdint.h>
#include <stdbool.h>
#include  <ili9341.h>

struct block_device;

int camera_init(struct block_device *bdev, int frame_width, int frame_height,
  int frame_rate, uint32_t bit_rate, bool preview_enable,
  struct ili9341_drawframe *preview_drawframe, int preview_width,
  int preview_height);

int camera_video_start(void);

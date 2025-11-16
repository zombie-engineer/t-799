#pragma once
#include <stdint.h>
#include <stdbool.h>

struct block_device;

int camera_init(struct block_device *bdev, int frame_width, int frame_height,
  int frame_rate, uint32_t bit_rate, bool preview_enable, int preview_width,
  int preview_height);

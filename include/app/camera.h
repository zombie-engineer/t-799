#pragma once
#include <stdint.h>

struct block_device;

int camera_init(struct block_device *bdev, int frame_width, int frame_height,
  int frame_rate, uint32_t bit_rate);

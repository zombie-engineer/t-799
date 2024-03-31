#pragma once
#include <stddef.h>

struct block_device;

struct block_device_ops {
  int (*read)(struct block_device *, char *buf, size_t start_sector,
    size_t num_sectors);

  int (*write)(struct block_device *, const void *src, size_t start_sector,
    size_t num_sectors);
};

struct block_device {
  struct block_device_ops ops;
  int sector_size;
  void *priv;
};

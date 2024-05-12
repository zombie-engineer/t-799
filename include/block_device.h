#pragma once
#include <stddef.h>
#include <stdbool.h>

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

void blockdev_scheduler_fn(void);

struct blockdev_io {
  struct block_device *dev;
  bool is_write;
  void *addr;
  size_t start_sector;
  size_t num_sectors;
  void (*cb)(int);
};

void blockdev_scheduler_push_io(struct blockdev_io *io);
void blockdev_scheduler_init(void);

#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <list.h>

struct block_device;

struct block_dev_write_stream {
  struct block_device *bd;
};

struct block_device_ops {
  int (*read)(struct block_device *, uint8_t *buf, uint64_t start_sector,
    uint32_t num_sectors);

  int (*write)(struct block_device *, const uint8_t *src, uint64_t start_sector,
    uint32_t num_sectors);

  int (*write_stream_open)(struct block_device *, uint64_t start_sector);

  int (*write_stream_write)(struct block_device *,
    struct block_dev_write_stream *, const void *data, size_t num_bytes);

  int (*write_stream_push)(struct block_device *,
    struct list_head *buflist);

  int (*block_erase)(struct block_device *, uint64_t start_sector,
    uint32_t num_sectors);
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
  uint64_t start_sector;
  uint32_t num_sectors;
  void (*cb)(int);
};

void blockdev_scheduler_push_io(struct blockdev_io *io);
void blockdev_scheduler_init(void);
void blockdev_scheduler_set_part_dev(struct block_device *bdev);

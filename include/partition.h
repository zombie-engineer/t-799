#pragma once
#include <stdint.h>
#include <block_device.h>

struct partition {
  /* embedded block device structure */
  struct block_device bdev;

  /*
   * block device that on top of which
   * this partition leaves.
   */
  struct block_device *blockdev;

  /*
   * start sector on underlying disk from
   * where this partition starts.
   */
  size_t start_sector;

  /*
   * next after last sector on underlying disk from
   * where this partition starts.
   */
  size_t end_sector;
};

void partition_init(struct partition *p, struct block_device *disk_dev,
  size_t start_sector, size_t end_sector);

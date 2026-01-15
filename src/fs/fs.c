#include <fs/fs.h>
#include <fs/fat32.h>
#include <partition.h>
#include <fs/mbr.h>
#include <common.h>
#include <errcode.h>
#include <printf.h>
#include <drivers/sd/emmc_bcm2835.h>
#include <stringlib.h>
#include <kmalloc.h>
#include <os_api.h>

static struct partition main_part;

int fs_init(struct block_device *blockdev,
  struct block_device **bdev_partition)
{
  int err;
  struct mbr *mbr;
  const struct mbr_entry *mbr_entry;
  size_t num_sectors, start_sector;

  mbr = kzalloc(sizeof(*mbr));
  if (!mbr)
    return ERR_MEMALLOC;

  err = blockdev->ops.read(blockdev, (uint8_t *)mbr, 0, 1);
  if (err != SUCCESS)
    goto out;

  mbr_print_summary(mbr);
  mbr_entry = mbr_get_partition(mbr, 1);
  if (!mbr_entry)
    goto out;

  start_sector = mbr_entry_get_lba(mbr_entry);
  num_sectors = mbr_entry_get_num_sectors(mbr_entry);
  partition_init(&main_part, blockdev,
    start_sector,
    start_sector + num_sectors);

  *bdev_partition = &main_part.bdev;

out:
  kfree(mbr);
  return err;
}

void fs_dump_partition(struct block_device *bdev_partition)
{
  struct partition *p = container_of(bdev_partition, struct partition,
    bdev);
  os_log("partition: start_sector:%d, end_sector:%d\r\n",
    p->start_sector, p->end_sector);
}

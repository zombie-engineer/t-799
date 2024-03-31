#include <fs/fs.h>
#include <fs/fat32.h>
#include <partition.h>
#include <fs/mbr.h>
#include <common.h>
#include <errcode.h>
#include <printf.h>
#include <bcm2835/bcm2835_emmc_dev.h>
#include <stringlib.h>
#include <kmalloc.h>

static struct block_device main_bdev;
static struct partition main_part;

int fs_init(struct block_device **b)
{
  int err;
  struct mbr *mbr;
  const struct mbr_entry *mbr_entry;
  size_t num_sectors, start_sector;

  bcm2835_emmc_block_device_init(&main_bdev);
  mbr = kzalloc(sizeof(*mbr));
  if (!mbr)
    return ERR_MEMALLOC;

  err = main_bdev.ops.read(&main_bdev, (char *)mbr, 0, 1);
  if (err != SUCCESS)
    goto out;

  mbr_print_summary(mbr);
  mbr_entry = mbr_get_partition(mbr, 1);
  if (!mbr_entry)
    goto out;

  start_sector = mbr_entry_get_lba(mbr_entry);
  num_sectors = mbr_entry_get_num_sectors(mbr_entry);
  partition_init(&main_part, &main_bdev,
    start_sector,
    start_sector + num_sectors);

  *b = &main_part.bdev;

out:
  kfree(mbr);
  return err;
}

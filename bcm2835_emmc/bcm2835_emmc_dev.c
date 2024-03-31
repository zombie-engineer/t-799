#include <bcm2835/bcm2835_emmc_dev.h>
#include <bcm2835/bcm2835_emmc.h>
#include <errcode.h>

static int bcm2835_emmc_dev_read(struct block_device *b, char *buf,
  size_t start_sector, size_t num_sectors)
{
  return bcm2835_emmc_read(start_sector, num_sectors, buf);
}

static int bcm2835_emmc_dev_write(struct block_device *b, const void *src,
  size_t start_sector, size_t num_sectors)
{
  return bcm2835_emmc_write(start_sector, num_sectors, src);
}

int bcm2835_emmc_block_device_init(struct block_device *bdev)
{
  bdev->ops.read = bcm2835_emmc_dev_read;
  bdev->ops.write = bcm2835_emmc_dev_write;
}

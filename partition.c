#include <partition.h>
#include <common.h>
#include <stringlib.h>

static int partition_read(struct block_device *b,
 char *buf, size_t start_sector, size_t num_sectors)
{
  struct partition *p = container_of(b, struct partition, bdev);

  return p->blockdev->ops.read(p->blockdev, buf,
    p->start_sector + start_sector, num_sectors);
}

static int partition_write(struct block_device *b,
 const void *buf, size_t start_sector, size_t num_sectors)
{
  struct partition *p = container_of(b, struct partition, bdev);

  return p->blockdev->ops.write(p->blockdev, buf,
    p->start_sector + start_sector,
    num_sectors);
}

void partition_init(struct partition *p, struct block_device *b,
  size_t start_sector, size_t end_sector)
{
  memset(p, 0, sizeof(*p));
  p->bdev.ops.read = partition_read;
  p->bdev.ops.write = partition_write;
  p->blockdev = b;
  p->start_sector = start_sector;
  p->end_sector = end_sector;
}

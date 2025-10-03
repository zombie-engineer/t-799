#include <partition.h>
#include <common.h>
#include <stringlib.h>

static int partition_read(struct block_device *b,
  uint8_t *buf, uint64_t start_sector, uint32_t num_sectors)
{
  struct partition *p = container_of(b, struct partition, bdev);

  return p->blockdev->ops.read(p->blockdev, buf,
    p->start_sector + start_sector, num_sectors);
}

static int partition_write(struct block_device *b,
 const uint8_t *buf, uint64_t start_sector, uint32_t num_sectors)
{
  struct partition *p = container_of(b, struct partition, bdev);

  return p->blockdev->ops.write(p->blockdev, buf,
    p->start_sector + start_sector,
    num_sectors);
}

static int partition_block_erase(struct block_device *b,
 uint64_t start_sector, uint32_t num_sectors)
{
  struct partition *p = container_of(b, struct partition, bdev);

  return p->blockdev->ops.block_erase(p->blockdev,
    p->start_sector + start_sector, num_sectors);
}

static int partition_write_stream_open(struct block_device *b,
  size_t start_sector)
{
  struct partition *p = container_of(b, struct partition, bdev);

  return p->blockdev->ops.write_stream_open(p->blockdev,
    p->start_sector + start_sector);
}

static int partition_write_stream_write(struct block_device *bd,
  struct block_dev_write_stream *s, const void *buf, size_t bufsz)
{
  struct partition *p = container_of(bd, struct partition, bdev);

  return p->blockdev->ops.write_stream_write(p->blockdev, s, buf, bufsz);
}

static int partition_write_stream_push(struct block_device *bd,
    struct list_head *buflist)
{
  struct partition *p = container_of(bd, struct partition, bdev);
  return p->blockdev->ops.write_stream_push(p->blockdev, buflist);
}

void partition_init(struct partition *p, struct block_device *b,
  size_t start_sector, size_t end_sector)
{
  memset(p, 0, sizeof(*p));
  p->bdev.ops.read = partition_read;
  p->bdev.ops.write = partition_write;
  p->bdev.ops.block_erase = partition_block_erase;
  p->bdev.ops.write_stream_open = partition_write_stream_open;
  p->bdev.ops.write_stream_write = partition_write_stream_write;
  p->bdev.ops.write_stream_push = partition_write_stream_push;
  p->blockdev = b;
  p->start_sector = start_sector;
  p->end_sector = end_sector;
}

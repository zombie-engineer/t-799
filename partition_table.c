#include <fs/mbr.h>
#include <common.h>
#include <errcode.h>
#include <printf.h>

const struct mbr_entry *mbr_get_first_valid_partition(
  const struct mbr *mbr)
{
  const struct mbr_entry *e;

  for (size_t i = 0; i < ARRAY_SIZE(mbr->entries); ++i) {
    e = &mbr->entries[i];
    if (e->part_type)
      return e;
  }
  return NULL;
}

const struct mbr_entry *mbr_get_partition(const struct mbr *mbr,
  size_t idx)
{
  const struct mbr_entry *e = &mbr->entries[idx];
  return e->part_type ? e : NULL;
}

int mbr_read(struct block_device *b, struct mbr *mbr)
{
  return b->ops.read(b, (char*)mbr, 0, 1);
}

void mbr_print_summary(struct mbr *mbr)
{
  int i;
  struct mbr_entry *e;
  uint32_t lba, num_sectors;

  for (i = 0; i < ARRAY_SIZE(mbr->entries); ++i) {
    e = &mbr->entries[i];
    lba = mbr_part_get_lba(mbr, i);
    num_sectors = mbr_part_get_num_sectors(mbr, i);
    printf("part%d: %02x, type:%02x, start:%08x, end:%08x\r\n", i + 1,
      e->status_byte,
      e->part_type,
      lba,
      lba + num_sectors);
  }
}

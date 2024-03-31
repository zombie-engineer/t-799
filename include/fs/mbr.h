#pragma once
#include <stdint.h>
#include <compiler.h>
#include <block_device.h>
#include <common.h>
#include <unaligned_access.h>

struct mbr_entry {
  char status_byte;
  char chs_first[3];
  char part_type;
  char chs_last[3];
  uint32_t lba;
  uint32_t num_sectors;
} PACKED;

struct mbr {
  char bootstap_code[446];
  struct mbr_entry entries[4];
  char boot_signature[2];
} PACKED;

STRICT_SIZE(struct mbr, 512);
STRICT_SIZE(struct mbr_entry, 16); 

static inline uint32_t mbr_entry_get_lba(const struct mbr_entry *e)
{
  return get_unaligned_32_le(&e->lba);
}

static inline uint32_t mbr_entry_get_num_sectors(const struct mbr_entry *e)
{
  return get_unaligned_32_le(&e->num_sectors);
}

const struct mbr_entry *mbr_get_partition(const struct mbr *mbr,
  size_t idx);

const struct mbr_entry *mbr_get_first_valid_partition(
  const struct mbr *mbr);

void mbr_print_summary(struct mbr *mbr);

static inline const struct mbr_entry *mbr_get_part_by_idx(const struct mbr *m,
  int part_idx)
{
  BUG_IF(part_idx > 3, "partition index too large");
  return &m->entries[part_idx];
}

static inline uint32_t mbr_part_get_lba(const struct mbr *mbr, int part_idx)
{
  const struct mbr_entry *e;
  e = mbr_get_part_by_idx(mbr, part_idx);
  return mbr_entry_get_lba(e);
}

static inline uint32_t mbr_part_get_num_sectors(const struct mbr *mbr,
  int part_idx)
{
  const struct mbr_entry *e = mbr_get_part_by_idx(mbr, part_idx);
  return mbr_entry_get_num_sectors(e);
}

#pragma once
#include <block_device.h>
#include <compiler.h>
#include <unaligned_access.h>
#include <common.h>
#include <printf.h>
#include <log.h>

#define FAT_MSDOS_NAME_LEN 11
#define FAT32_SECTOR_SIZE 512

#define FAT32_FAT_ENTRY_SIZE 4
#define FAT32_ENTRY_END_OF_CHAIN     0x0fffffff
#define FAT32_ENTRY_END_OF_CHAIN_ALT 0x0ffffff8
#define FAT32_ENTRY_FREE             0x00000000
#define FAT32_ENTRY_ID               0x0ffffff8

#define FAT_ENTRY_DELETED_MARK 0xe5
#define FAT32_DATA_FIRST_CLUSTER 2

#define FAT_FILE_ATTR_READONLY     (1<<0)
#define FAT_FILE_ATTR_HIDDEN       (1<<1)
#define FAT_FILE_ATTR_SYSTEM       (1<<2)
#define FAT_FILE_ATTR_VOLUME_LABEL (1<<3)
#define FAT_FILE_ATTR_SUBDIRECTORY (1<<4)
#define FAT_FILE_ATTR_ARCHIVE      (1<<5)
#define FAT_FILE_ATTR_DEVICE       (1<<6)
#define FAT_FILE_ATTR_RESERVED     (1<<7)

#define FAT_FILE_ATTR_VFAT_LFN 0xf

#define FAT_LFN_FIELD_LEN_1 10
#define FAT_LFN_FIELD_LEN_2 12
#define FAT_LFN_FIELD_LEN_3 4
struct vfat_lfn_entry {
  char seq_num;
  char name_part_1[FAT_LFN_FIELD_LEN_1];

  /* should be 0x0f */
  char file_attributes;

  /* should be 0x00 */
  char file_type;
  uint8_t checksum;
  char name_part_2[FAT_LFN_FIELD_LEN_2];

  /* should be 0x00 */
  uint16_t first_cluster;

  char name_part_3[FAT_LFN_FIELD_LEN_3];
} PACKED;

#define FAT_LFN_NAME_LEN  (FAT_LFN_FIELD_LEN_1 + FAT_LFN_FIELD_LEN_2 \
  + FAT_LFN_FIELD_LEN_3)

STRICT_SIZE(struct vfat_lfn_entry, 32);

struct fat_dentry {
  char filename_short[8];
  char extension[3];
  char file_attributes;
  /*  */
  char dos_attrs;
  char undelete_byte;
  uint16_t creation_time;
  uint16_t creation_date;
  uint16_t last_access_date;
  uint16_t cluster_addr_high;
  uint16_t last_modify_time;
  uint16_t last_modify_date;
  uint16_t cluster_addr_low;
  uint32_t file_size;
} PACKED;

static inline size_t fat32_calc_num_dentries(const char *filename)
{
  size_t i;
  size_t len_after_dot = 0;
  size_t len = strnlen(filename, 128);
  size_t len_before_dot = len;

  if (len_before_dot == 128)
    printf("Invalid file name provided\n");

  for (i = len; i > 0; --i) {
    if (filename[i - 1] == '.') {
      len_before_dot = i;
      len_after_dot = len_before_dot - i;
      break;
    }
  }

  /*
   * Technically we don't need long name if filename fits in dos name field
   * in dentry, but at other OS implementations do long name as well
   */
  if (len_before_dot <= 8 && len_after_dot <= 3)
    return 2;

  return (len + FAT_LFN_NAME_LEN - 1) / FAT_LFN_NAME_LEN + 1;
}

static inline uint32_t fat32_dentry_get_cluster(const struct fat_dentry *d)
{
  return (get_unaligned_16_le(&d->cluster_addr_high) << 16) |
    get_unaligned_16_le(&d->cluster_addr_low);
}

static inline void fat_dentry_set_cluster(struct fat_dentry *d,
  uint32_t cluster)
{
  set_unaligned_16_le(&d->cluster_addr_high, (cluster >> 16) & 0xffff);
  set_unaligned_16_le(&d->cluster_addr_low, cluster & 0xffff);
}

static inline uint32_t fat32_dentry_get_file_size(const struct fat_dentry *d)
{
  return (get_unaligned_32_le(&d->file_size));
}

static inline void fat_dentry_set_file_size(struct fat_dentry *d,
  uint32_t file_size)
{
  return set_unaligned_32_le(&d->file_size, file_size);
}

static inline bool fat32_dentry_is_subdir(struct fat_dentry *d)
{
  return d->file_attributes & FAT_FILE_ATTR_SUBDIRECTORY;
}

static inline bool fat32_dentry_is_valid(struct fat_dentry *d)
{
  return d->filename_short[0] &&
    (fat32_dentry_get_cluster(d) || fat32_dentry_get_file_size(d) == 0);
}

static inline bool fat32_dentry_is_deleted(const struct fat_dentry *d)
{
  const uint8_t *p = (const uint8_t *)d->filename_short;
  uint8_t c = p[0];
  return c == FAT_ENTRY_DELETED_MARK;
}

/*
 * called during creating a new file and allocating new directory entry in
 * parent dir. Returns true if entry is available for allocation. Only works
 * for non lfn entries
 */
static inline bool fat32_dentry_allocatable(const struct fat_dentry *d)
{
  return d->filename_short[0] == 0 || fat32_dentry_is_deleted(d);
}

static inline bool fat32_dentry_is_volume_label(const struct fat_dentry *d)
{
  return d->file_attributes & FAT_FILE_ATTR_VOLUME_LABEL;
}

static inline bool fat_dentry_is_lfn_entry(const struct fat_dentry *d)
{
  return (d->file_attributes & FAT_FILE_ATTR_VFAT_LFN)
    == FAT_FILE_ATTR_VFAT_LFN;
}

static inline bool fat32_dentry_is_end_mark(const struct fat_dentry *d)
{
  char *ptr = (char *)d;
  char *end = ptr + sizeof(*d);
  while(ptr < end) {
    if (*ptr++)
      return false;
  }
  return true;
}

static inline struct fat_dentry *fat32_dentry_next(
  struct fat_dentry *d,
  struct fat_dentry *d_end,
  struct fat_dentry **out_d_first)
{
  while(d_end - d > 1) {
    d++;
    if (fat_dentry_is_lfn_entry(d)) {
      if (!*out_d_first)
        *out_d_first = d;
      continue;
    }

    if (fat32_dentry_is_volume_label(d))
      continue;

    if (fat32_dentry_is_deleted(d)) {
      *out_d_first = NULL;
      continue;
    }

    if (fat32_dentry_is_valid(d))
      return d;

    if (fat32_dentry_is_end_mark(d))
      return NULL;

    /* ?? invalid record */
    puts("fat32_dentry_next: found invalid dentry record:" __endline);
    // hexdump_memory_ex("--", 32, d, sizeof(*d));
  }
  return d_end;
}

STRICT_SIZE(struct fat_dentry, 32);

struct fat32_fs_info {
  /* "RRaA" */
  char signature[4];
  char reserved[480];
  /* "rrAa" */
  char signature2[4];
  uint32_t last_known_free_clusters;
  uint32_t last_allocated_cluster;
  char reserved2[12];
  /* 00 00 55 AA */
  char signature3[4];
} PACKED;

STRICT_SIZE(struct fat32_fs_info, 512);

struct fat_dos20_bpb {
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t num_reserved_sectors;
  uint8_t num_fats;
  /* 0 for FAT32 */
  uint16_t max_root_entries;
  /* 0 for FAT32 */
  uint16_t total_sectors;
  uint8_t media_descriptor;
  /* 0 for FAT32 */
  uint16_t sectors_per_fat;
} PACKED;

STRICT_SIZE(struct fat_dos20_bpb, 13);

struct fat_dos331_bpb {
  struct fat_dos20_bpb bpb_dos20;
  uint16_t sectors_per_track;
  uint16_t heads_per_disk;
  uint32_t num_hidden_sectors;
  uint32_t total_logical_sectors;
} PACKED;

STRICT_SIZE(struct fat_dos331_bpb, 25);

/* FAT32 Extended BIOS parameter block (EBPB) */
struct fat32_ebpb {
  struct fat_dos331_bpb bpb_dos331;
  uint32_t sectors_per_fat;
  char drive_desc[2];
  char version[2];
  uint32_t root_dir_cluster;
  uint16_t fs_info_sector;
  uint16_t fat_cpy_sector;
  char res[12];
  char misc_exfat;
  char misc_unused1;
  char misc_unused2;
  char volume_id[4];
  char volume_label[11];
  char fs_type_name[8];
} PACKED;

STRICT_SIZE(struct fat32_ebpb, 79);

struct fat32_boot_sector {
  char jmp[3];
  char oem_name[8];
  struct fat32_ebpb ebpb;
  char bootcode[419];
  char drive_number;
  /* 55 aa */
  char signature[2];
} PACKED;

STRICT_SIZE(struct fat32_boot_sector, 512);

struct fat32_fs {
  struct block_device *bdev;
  struct fat32_boot_sector *boot_sector;
};

static inline uint16_t fat32_get_num_reserved_sectors(
  const struct fat32_fs *fs)
{
  return get_unaligned_16_le(
    &fs->boot_sector->ebpb.bpb_dos331.bpb_dos20.num_reserved_sectors);
}

static inline uint32_t fat32_get_root_dir_cluster(const struct fat32_fs *fs)
{
  return get_unaligned_32_le(&fs->boot_sector->ebpb.root_dir_cluster);
}

static inline uint16_t fat32_get_num_root_entries(const struct fat32_fs *fs)
{
  return get_unaligned_16_le(
    &fs->boot_sector->ebpb.bpb_dos331.bpb_dos20.max_root_entries);
}

static inline size_t fat32_sector_size(const struct fat32_fs *fs)
{
  return get_unaligned_16_le(
    &fs->boot_sector->ebpb.bpb_dos331.bpb_dos20.bytes_per_sector);
}

static inline size_t fat32_fat_entries_per_sector(const struct fat32_fs *fs)
{
  return fat32_sector_size(fs) / sizeof(uint32_t);
}

static inline size_t fat32_get_dentries_per_sector(const struct fat32_fs *fs)
{
  return fat32_sector_size(fs) / sizeof(struct fat_dentry);
}

static inline uint32_t fat32_sectors_per_cluster(const struct fat32_fs *fs)
{
  return fs->boot_sector->ebpb.bpb_dos331.bpb_dos20.sectors_per_cluster;
}

static inline uint32_t fat32_bytes_per_cluster(const struct fat32_fs *fs)
{
  return fat32_sector_size(fs) * fat32_sectors_per_cluster(fs);
}

static inline size_t fat32_entries_per_fat_sector(const struct fat32_fs *fs)
{
  return fat32_sector_size(fs) / sizeof(uint32_t);
}

static inline uint16_t fat32_get_sectors_per_fat(const struct fat32_fs *fs)
{
  return get_unaligned_32_le(&fs->boot_sector->ebpb.sectors_per_fat);
}

static inline size_t fat32_fat_start_sector_index(const struct fat32_fs *fs,
  int fat_idx)
{
  return fat32_get_num_reserved_sectors(fs) +
    fat_idx * fat32_get_sectors_per_fat(fs);
}

static inline uint8_t fat32_get_num_fats(const struct fat32_fs *fs)
{
  return fs->boot_sector->ebpb.bpb_dos331.bpb_dos20.num_fats;
}

static inline uint32_t fat32_get_num_fat_sectors(const struct fat32_fs *fs)
{
  return fat32_get_num_fats(fs) * fat32_get_sectors_per_fat(fs);
}

static inline size_t fat32_num_root_dir_sectors(const struct fat32_fs *fs)
{
  return (fat32_get_num_root_entries(fs) * sizeof(struct fat_dentry))
    / fat32_sector_size(fs);
}

static inline uint64_t fat32_get_data_start_sector(const struct fat32_fs *fs)
{
  return fat32_get_num_reserved_sectors(fs) + fat32_get_num_fat_sectors(fs);
}

static inline uint32_t fat32_get_root_dir_sector(const struct fat32_fs *fs)
{
  return fat32_get_data_start_sector(fs);
}

static inline size_t fat32_get_file_size(const struct fat_dentry *d)
{
  return (size_t)d->file_size;
}

static inline size_t fat32_cluster_to_sector_idx(const struct fat32_fs *fs,
  size_t cluster_idx)
{
  /*
   * According to the formula on FAT32 Wikipedia page to convert cluster 
   * index to fist sector index
   */
  return fat32_get_num_reserved_sectors(fs) +
    fat32_get_num_fats(fs) * fat32_get_sectors_per_fat(fs) +
    fat32_num_root_dir_sectors(fs) +
    (cluster_idx - FAT32_DATA_FIRST_CLUSTER) *
    fat32_sectors_per_cluster(fs);
}

static inline size_t fat32_sector_to_cluster_idx(const struct fat32_fs *fs,
  size_t sector_idx)
{
  size_t sector = sector_idx - fat32_get_num_reserved_sectors(fs) -
    fat32_get_num_fats(fs) * fat32_get_sectors_per_fat(fs) -
    fat32_num_root_dir_sectors(fs) +
    FAT32_DATA_FIRST_CLUSTER * fat32_sectors_per_cluster(fs);

  return sector / fat32_sectors_per_cluster(fs);
}


int fat32_fs_open(struct block_device *bdev, struct fat32_fs *fs);

int fat32_create(const struct fat32_fs *fs, const char *filepath, bool is_dir,
  bool recursive);

int fat32_write(const struct fat32_fs *fs, const char *filename, size_t offset,
    size_t size, const void *data);

void fat32_summary(const struct fat32_fs *fs);

int fat32_read(const struct fat32_fs *fs, const struct fat_dentry *d,
  uint8_t *dst, size_t offset, size_t count);

int fat32_ls(const struct fat32_fs *fs, const char *dirpath);

int fat32_resize(const struct fat32_fs *fs, const char *file, size_t new_size);

int fat32_dump_file_cluster_chain(const struct fat32_fs *fs,
  const char *filename);

void fat32_dump_each_fat(const struct fat32_fs *fs);

void fat32_dump_fat(const struct fat32_fs *fs, int fat_idx);

struct fat_dentry_loc {
  /* sector index, containing dentry */
  size_t start_sector_idx;

  /* entry index relative to sector 'start_sector_idx' */
  size_t start_dentry_idx;

  /* how much dentries describe the file */
  size_t num_dentries;
};

int fat32_lookup(const struct fat32_fs *fs, const char *filepath,
  struct fat_dentry_loc *loc, struct fat_dentry *d);

void fat32_init(void);

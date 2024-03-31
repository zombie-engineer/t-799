#include <fs/fat32.h>
#include <common.h>
#include <stringlib.h>
#include <errcode.h>

#define FAT_INVAL_DENTRY_IDX 0xffffffff
#define FAT_SCRATCHBUF_CNT 8

typedef enum {
  FAT_DIR_ITER_STOP = 0,
  FAT_DIR_ITER_CONTINUE
} fat_dir_iter_result;

typedef enum {
  FAT_FILE_ITER_STOP = 0,
  FAT_FILE_ITER_PROCEED
} fat_file_iter_result;

typedef enum {
  FAT_ITER_FAT_SECTORS_STOP = 0,
  FAT_ITER_FAT_SECTORS_PROCEED
} fat_iter_fat_sectors_result;


typedef fat_file_iter_result (*fat_file_iter_cb)(size_t cluster_idx,
  size_t sector_idx, void *);

typedef bool (*fat_foreach_cluster_cb)(const struct fat32_fs *,
  size_t cluster_idx, void *arg);

struct fat32_boot_sector fat32_boot_sector ALIGNED(4);

static char fat_scratchbufs[FAT_SCRATCHBUF_CNT][FAT32_SECTOR_SIZE] ALIGNED(512);
static char fat_scratchbuf_mask = 0;

static char *fat_scratchbuf_alloc(void)
{
  size_t i;
  for (i = 0; i < 8; ++i) {
    if (!(fat_scratchbuf_mask & (1<<i))) {
      fat_scratchbuf_mask |= (1<<i);
      return fat_scratchbufs[i];
    }
  }
  return NULL;
}

static void fat_scratchbuf_free(char *scratchbuf)
{
  if ((uint64_t)scratchbuf & (FAT32_SECTOR_SIZE - 1))
    return;
  size_t idx = (scratchbuf - fat_scratchbufs[0]) / FAT32_SECTOR_SIZE;
  if (idx > 7)
    return;

  fat_scratchbuf_mask &= ~(1<<idx);
}

/*
 * Simple implementation of copying wide chars into 1-byte ASCII string. This
 * assumes wide chars are combined as 0 byte + ascii byte actually
 */
static size_t wide_to_simple_char(char *dst, size_t dst_sz,
  const char *src,
  size_t src_sz)
{
  size_t i, max_idx;
  if (dst_sz < 1)
    return 0;

  max_idx = MIN(src_sz / 2, dst_sz - 1);
  for (i = 0; i < max_idx && src[i * 2]; ++i)
    dst[i] = src[i * 2];

  dst[i] = 0;
  return i;
}

static const char *fat_lfn_fill_name(struct vfat_lfn_entry *lfn,
  const char *src)
{
  size_t i;
  char *dst, *dst_end;
  size_t field_idx = -1;
  char c;
  bool fill_with_ff = false;

  struct {
    char *field;
    size_t size;
  } fields[] = {
    { .field = lfn->name_part_1, .size = sizeof(lfn->name_part_1) },
    { .field = lfn->name_part_2, .size = sizeof(lfn->name_part_2) },
    { .field = lfn->name_part_3, .size = sizeof(lfn->name_part_3) }
  };

  dst = dst_end = NULL;
  i = 0;

  while(1) {
    if (dst == dst_end) {
      field_idx++;
      if (field_idx == ARRAY_SIZE(fields))
        break;

      dst = fields[field_idx].field;
      dst_end = dst + fields[field_idx].size;
    }

    if (fill_with_ff) {
      *dst++ = 0xff;
      *dst++ = 0xff;
      continue;
    }

    c = src[i++];
    if (c) {
      *dst++ = c;
      *dst++ = 0;
      i;
    } else {
      *dst++ = 0;
      *dst++ = 0;
      fill_with_ff = true;
    }
  }
}

static void fat_dentry_get_shortname_string(const struct fat_dentry *d,
  char *namebuf, size_t namebuf_len)
{
  memcpy(namebuf, d->filename_short, 8);
  memcpy(namebuf + 8, d->extension, 3);
}

static uint8_t fat_calc_shortname_checksum(const struct fat_dentry *d)
{
  char shortname[FAT_MSDOS_NAME_LEN];
  int i;
  unsigned char sum = 0;
  bool padding = false;
  char c;
  const char *src = shortname;

  fat_dentry_get_shortname_string(d, shortname, sizeof(shortname));

  for (i = FAT_MSDOS_NAME_LEN; i; i--) {
    if (!padding) {
      c = *src++;
      if (!c)
        padding = true;
    }
    else
      c = ' ';

    sum = ((sum & 1) << 7) + (sum >> 1) + c;
  }

  return sum;
}


static int fat32_get_next_cluster_num(const struct fat32_fs *f,
  uint32_t cluster_idx, size_t *next_cluster_idx)
{
  int err;
  uint32_t fat_start_sector;
  /* FAT entry - single entry in FAT that describes cluster chaining */
  uint32_t fat_entries_per_sector;
  /* index of a sector in FAT where required cluster is described */
  uint64_t fat_entry_sector;
  const uint32_t *fat_sector;
  char *buf;

  fat_start_sector = fat32_get_num_reserved_sectors(f);
  fat_entries_per_sector = fat32_sector_size(f) / FAT32_FAT_ENTRY_SIZE;
  fat_entry_sector = cluster_idx / fat_entries_per_sector;
  if (fat_entry_sector > fat32_get_sectors_per_fat(f)) {
    printf("fat32_get_next_cluster_num: cluster index %d too big\r\n",
      cluster_idx);
    return ERR_GENERIC;
  }

  buf = fat_scratchbuf_alloc();
  if (!buf) {
    printf("Failed to allocate buffer for reading\n");
    return ERR_MEMALLOC;
  }

  err = f->bdev->ops.read(f->bdev, buf, fat_start_sector + fat_entry_sector,
    1);
  if (err < 0) {
    printf("fat32_get_next_cluster_num: error reading first sector\r\n");
    fat_scratchbuf_free(buf);
    return err;
  }

  fat_sector = (const uint32_t *)buf;
  *next_cluster_idx = fat_sector[cluster_idx % fat_entries_per_sector];
  fat_scratchbuf_free(buf);
  return SUCCESS;
}

/*
 * Iterate every cluster in a cluster chain starting form 'start_cluster_idx'
 */
static int fat_foreach_cluster(const struct fat32_fs *fs,
  size_t start_cluster_idx, fat_foreach_cluster_cb cb, void *arg)
{
  int err;
  size_t cluster_idx = start_cluster_idx;

  if (!start_cluster_idx)
    return ERR_INVAL;

  while(1) {
    if (!cb(fs, cluster_idx, arg))
      break;

    err = fat32_get_next_cluster_num(fs, cluster_idx, &cluster_idx);
    if (err != SUCCESS)
      return err;

    if (cluster_idx == FAT32_ENTRY_END_OF_CHAIN ||
      cluster_idx == FAT32_ENTRY_END_OF_CHAIN_ALT)
      break;
  }
  return SUCCESS;
}

int fat32_fs_open(struct block_device *bdev, struct fat32_fs *f)
{
  char oem_buf[9];
  int err;
  int root_cluster_idx;
  uint32_t cluster_size;
  err = bdev->ops.read(bdev, (char*)&fat32_boot_sector, 0,
    1);
  if (err < 0) {
    printf("fat32_open: error reading first sector\r\n");
    return err;
  }

  f->boot_sector = &fat32_boot_sector;
  f->bdev = bdev;
  memcpy(oem_buf, fat32_boot_sector.oem_name,
    sizeof(fat32_boot_sector.oem_name));
  oem_buf[8] = 0;
  root_cluster_idx = fat32_get_root_dir_cluster(f);
  cluster_size = fat32_bytes_per_cluster(f);
  printf("fat32_open: success. OEM: \"%s\", root_cluster:%d,"
    " cluster_sz:%d, fat_sectors:%d\r\n",
    oem_buf,
    root_cluster_idx,
    cluster_size,
    fat32_get_sectors_per_fat(f));
  return 0;
}

static inline size_t fat32_fill_long_filename(char *dst, size_t dst_sz,
  struct vfat_lfn_entry *le)
{
  size_t field_idx, max_dst_len;
  size_t num_copied;
  const char *src;
  const char *dst_orig = dst;
  const struct {
    const char *field;
    size_t size;
  } fields[] = {
    { .field = le->name_part_1, .size = sizeof(le->name_part_1) },
    { .field = le->name_part_2, .size = sizeof(le->name_part_2) },
    { .field = le->name_part_3, .size = sizeof(le->name_part_3) }
  };

  if (dst_sz == 0)
    return 0;

  if (dst_sz == 1)
    goto out;

  for (field_idx = 0; field_idx < ARRAY_SIZE(fields); ++field_idx) {
    src = fields[field_idx].field;
    max_dst_len = MIN(fields[field_idx].size / 2, dst_sz - 1);
    for (size_t i = 0; i < max_dst_len ; ++i) {
      char c = src[i * 2];
      if (!c)
        goto out;
      *dst++ = c;
    }
  }

out:
  *dst = 0;
  return dst - dst_orig;
}

static inline int fat32_fill_short_filename(char *dst, size_t dst_sz,
  const struct fat_dentry *d)
{
  int i;
  char *ptr, *end;
  ptr = dst;
  end = dst + dst_sz;

  for (i = 0; i < sizeof(d->filename_short) && ptr < end; ++i) {
    if (!d->filename_short[i])
      break;
    *ptr++ = d->filename_short[i];
  }

  if (ptr < end && d->extension[0])
    *ptr++ = '.';

  for (i = 0; i < sizeof(d->extension) && ptr < end; ++i) {
    if (d->extension[i] && d->extension[i] != ' ')
      *ptr++ = d->extension[i];
  }

  *ptr = 0;
  return ptr - dst;
}

static void fat32_dentry_get_long_filename(char *dst, size_t dst_sz,
  struct fat_dentry *dentries, size_t num_dentries)
{
  size_t i;
  size_t count;
  size_t num_lfn_entries = num_dentries - 1;

  /*
   * dentries is an array of length num_dentries:
   * - dentries[0, 1, 2, ... num_dentries - 2] are vfat lfn (long file name)
   * - dentries[num_entries - 1] - last dentry and also the only actual 
   *   dentry, holding a dos short filename and all file attributes
   * if num_dentries is 1 - only short filename is present
   * if num_dentries is > 1 - first go LFNs, then last goes the dentry
   */

  if (num_lfn_entries) {
    for (i = num_lfn_entries; i; --i) {
      size_t lfn_dentry_idx = i - 1;
      count = fat32_fill_long_filename(dst, dst_sz,
        (struct vfat_lfn_entry *)(dentries + lfn_dentry_idx));

      if (count > dst_sz) {
        printf("ERROR\b");
        return;
      }
      dst += count;
      dst_sz -= count;
    }
  }
  else
    fat32_fill_short_filename(dst, dst_sz, dentries);
}

static void fat32_dentry_get_shortname(char *namebuf, size_t namebuf_len,
  struct fat_dentry *d)
{
  size_t i;
  char c;
  char *ptr = namebuf;

  if (namebuf_len < sizeof(d->filename_short))
    return;

  for (i = 0; i < sizeof(d->filename_short); ++i) {
    c = d->filename_short[i];
    if (c == ' ')
      break;
    *ptr++ = c;
  }

  if (d->extension[0] != ' ' && d->extension[0] != 0) {
    *ptr++ = '.';
    for (i = 0; i < 3; ++i) {
      if (d->extension[i] == ' ')
        break;
      *ptr++ = d->extension[i];
    }
  }
  *ptr++ = 0;
}

fat_dir_iter_result fat32_dentry_print(size_t sector_idx,
  size_t first_dentry_idx, struct fat_dentry *dentries, size_t num_dentries,
  void *cb_arg)
{
  uint32_t cluster;
  char namebuf[128];
  char shortname[16];
  struct fat_dentry *main_dentry = dentries + num_dentries - 1;
  const struct vfat_lfn_entry *first_lfn = (const void *)
    (num_dentries > 1 ? dentries : NULL);

  uint8_t calculated_checksum = fat_calc_shortname_checksum(main_dentry);

  fat32_dentry_get_shortname(shortname, sizeof(shortname), main_dentry);
  fat32_dentry_get_long_filename(namebuf, sizeof(namebuf), dentries,
    num_dentries);

  cluster = fat32_dentry_get_cluster(main_dentry);
  printf("'%s'/'%s' size: %d, cluster: %d, attr: %02x, %s,0x%02x/0x%02x\r\n",
    shortname, namebuf, main_dentry->file_size, cluster,
    main_dentry->file_attributes,
    fat32_dentry_is_subdir(main_dentry) ? "DIR" : "FILE",
    first_lfn ? first_lfn->checksum : 0, 
    calculated_checksum);

  return FAT_DIR_ITER_CONTINUE;
}

/*
 * Iterate symbols of filepath until end of string of slash-delimeter '/'
 * returns number of symbols from the given position to end of subpath,
 * subpath is terminated with '/', zero-terminator or end pointer, whichever
 * is met first.
 */
static size_t filepath_get_next_element(const char **path)
{
  size_t len;
  const char *p = *path;

  while(*p && *p != '/')
    p++;

  len = p - *path;
  *path = p;
  return len;
}

static void filepath_get_last_element(const char *filepath, const char **fname)
{
  const char *p = filepath;
  const char *last_start = p;

  while(*p) {
    if (*p == '/') {
      last_start = p + 1;
    }
    p++;
  }
  *fname = last_start;
}

typedef fat_dir_iter_result (*fat_dir_iter_cb)(size_t sector_idx,
  size_t first_dentry_idx, struct fat_dentry *dentries, size_t num_dentries,
  void *);

struct fat_iter_dents_in_cluster_ctx {
  int err;
  fat_dir_iter_cb cb;
  void *cb_arg;
  size_t first_dentry_idx;
  struct fat_dentry dentries;
  size_t num_dentries;
};

static bool fat_iter_dents_in_cluster(const struct fat32_fs *fs,
  size_t cluster_idx, void *arg)
{
  int err;
  struct fat_iter_dents_in_cluster_ctx *ctx = arg;
  struct fat_dentry *d, *d_end, *d_first;
  struct fat_dentry dentries[8];
  uint32_t sector_size;
  size_t start_sect_idx, sect_idx, end_sect_idx;
  char *buf = NULL;
  bool should_continue = false;

  size_t start_dentry_idx;
  d = NULL;
  sector_size = fat32_sector_size(fs);

  start_sect_idx =  fat32_cluster_to_sector_idx(fs, cluster_idx);
  sect_idx = start_sect_idx;
  end_sect_idx = sect_idx + fat32_sectors_per_cluster(fs);

  buf = fat_scratchbuf_alloc();
  if (!buf) {
    printf("Failed to allocate buffer for reading\n");
    ctx->err = ERR_MEMALLOC;
    goto out_nomalloc;
  }

  while(sect_idx < end_sect_idx) {
    err = fs->bdev->ops.read(fs->bdev, buf, sect_idx, 1);
    if (err < 0) {
      ctx->err = err;
      printf("Error reading sector %ld, err: %d\n", sect_idx, err);
      goto out;
    }

    d = (struct fat_dentry *)buf;
    d_end = (struct fat_dentry *)(buf + sector_size);
    d--;

    while(1) {
      d_first = NULL;
      /* next directory */
      d = fat32_dentry_next(d, d_end, &d_first);
      size_t num_dentries = (d_first ? d - d_first : 0) + 1;
      if (d_first) {
        if (d < d_first || num_dentries > ARRAY_SIZE(dentries))
        {
          ctx->err = ERR_GENERIC;
          printf("Corrupted filesystem\r\n");
          goto out;
        }

        for (size_t i = 0; i < num_dentries; i++)
          memcpy(dentries + i, d_first + i, sizeof(*d_first));
      }
      else
        d_first = d;

      start_dentry_idx = d_first - (struct fat_dentry *)buf;

      if (d == d_end)
        break;

      /* no more entries */
      if (!d)
        goto out;

      /* iteration reached  end of sector, proceed to next cluster */
      if (d == d_end)
        break;

      if (ctx->cb(sect_idx, start_dentry_idx, d_first, num_dentries,
        ctx->cb_arg) == FAT_DIR_ITER_STOP)
        goto out;
    }
    sect_idx++;
  }

  should_continue = true;

out:
  fat_scratchbuf_free(buf);

out_nomalloc:
  return should_continue;
}

static int fat_iter_dentries(const struct fat32_fs *fs,
  uint32_t start_cluster_idx,
  fat_dir_iter_cb cb,
  void *cb_arg)
{
  int err;

  struct fat_iter_dents_in_cluster_ctx dir_iter_ctx = {
    .err = SUCCESS,
    .cb = cb,
    .cb_arg = cb_arg,
    .num_dentries = 0,
    .first_dentry_idx = 0
  };

  err = fat_foreach_cluster(fs, start_cluster_idx, fat_iter_dents_in_cluster,
    &dir_iter_ctx);

  if (err != SUCCESS) {
    printf("FAT cluster walk failed %d\n", err);
    return err;
  }

  if (dir_iter_ctx.err != SUCCESS) {
    printf("FAT dir iter failed %d\n", dir_iter_ctx.err);
    return dir_iter_ctx.err;
  }

  return SUCCESS;
}

struct fat_find_by_name_ctx {
  const struct fat32_fs *fs;
  const char *filename;
  int err;
  struct fat_dentry dentries[8];
  size_t num_dentries;
  struct fat_dentry_loc *loc;
};

static fat_dir_iter_result fat_find_dentry_by_name(size_t sector_idx,
  size_t start_dentry_idx, struct fat_dentry *dentries, size_t num_dentries,
  void *arg)
{
  char namebuf[128];
  struct fat_find_by_name_ctx *ctx = arg;

  fat32_dentry_get_long_filename(namebuf, sizeof(namebuf), dentries,
    num_dentries);

  if (strcmp(namebuf, ctx->filename) == 0) {
    ctx->loc->start_sector_idx = sector_idx;
    ctx->loc->start_dentry_idx = start_dentry_idx;
    ctx->loc->num_dentries = num_dentries;

    for (size_t i = 0; i < num_dentries; ++i)
      memcpy(ctx->dentries + i, dentries + i, sizeof(struct fat_dentry));
    ctx->num_dentries = num_dentries;
    ctx->err = SUCCESS;
    return FAT_DIR_ITER_STOP;
  }

  return FAT_DIR_ITER_CONTINUE;
}

int fat32_lookup(const struct fat32_fs *fs, const char *filepath,
  struct fat_dentry_loc *loc,
  struct fat_dentry *out_dentry)
{
  int err;
  size_t n;
  const char *p;
  uint32_t parent_cluster0;
  char namebuf[128];
  struct fat_find_by_name_ctx ctx;
  struct fat_dentry *current_dentry;
  bool last_is_dir;

  ctx.fs = fs;

  if (filepath[0] != '/') {
    printf("fat32_lookup: invalid argument: filepath: %s\r\n", filepath);
    return ERR_INVAL;
  }

  p = filepath + 1;
  parent_cluster0 = fat32_get_root_dir_cluster(fs);
  last_is_dir = true;

  while(1) {
    n = filepath_get_next_element(&p);
    if (!n)
      break;

    if (!last_is_dir) {
      printf("fat32_lookup: is not dir\r\n");
      return ERR_INVAL;
    }

    strncpy(namebuf, p - n, n);
    namebuf[n] = 0;
    printf("fat32_lookup: %s\r\n", namebuf);
    if (*p == '/')
      p++;

    ctx.err = ERR_NOT_FOUND;
    ctx.filename = namebuf;
    ctx.loc = loc;

    err = fat_iter_dentries(fs, parent_cluster0, fat_find_dentry_by_name, &ctx);

    if (err != SUCCESS) {
      printf("fat32_lookup: failed to iterate directory entries %out_dentry\r\n", err);
      return err;
    }

    if (ctx.err != SUCCESS) {
      if (ctx.err != ERR_NOT_FOUND)
        printf("fat32_lookup: unexpected error during directory "
          "iteration: %out_dentry\r\n", ctx.err);
      return ctx.err;
    }

    current_dentry = &ctx.dentries[ctx.num_dentries - 1];
    last_is_dir = fat32_dentry_is_subdir(current_dentry);
    parent_cluster0 = fat32_dentry_get_cluster(current_dentry);
  }

  memcpy(out_dentry, current_dentry, sizeof(struct fat_dentry));
  return SUCCESS;
}

static size_t fat_calc_num_clusters_for_data_size(const struct fat32_fs *fs,
  size_t size)
{
  size_t bytes_per_cluster = fat32_bytes_per_cluster(fs);
  return (size + bytes_per_cluster - 1) / bytes_per_cluster;
}

/* Update entry in a previous fat sector */
static int fat_set_cluster_entry(const struct fat32_fs *fs,
  size_t cluster_idx, size_t next_cluster_idx)
{
  int err;
  char *buf = fat_scratchbuf_alloc();
  const size_t fat_entries_per_sect = fat32_fat_entries_per_sector(fs);

  size_t fat_sector_idx = cluster_idx / fat_entries_per_sect +
    fat32_fat_start_sector_index(fs, 0);

  size_t fat_entry_idx = cluster_idx % fat_entries_per_sect;

  if (!buf) {
    printf("Failed to allocate buffer 2 for reading\n");
    return ERR_MEMALLOC;
  }

  uint32_t *fat = (uint32_t *)buf;
  err = fs->bdev->ops.read(fs->bdev, buf, fat_sector_idx, 1);
  if (err != SUCCESS) {
    printf("Error reading sector %ld, err:%d\n", fat_sector_idx, err);
    goto out;
  }
  fat[fat_entry_idx] = next_cluster_idx;
  err = fs->bdev->ops.write(fs->bdev, buf, fat_sector_idx, 1);
  if (err != SUCCESS) {
    printf("Error writing to sector %ld, err:%d\n", fat_sector_idx, err);
    goto out;
  }
out:
  fat_scratchbuf_free(buf);
  return err;
}

static int fat_dir_entry_mod(const struct fat32_fs *fs,
  const struct fat_dentry_loc *l, size_t first_cluster_idx, size_t file_size)
{
  int err;

  size_t dentry_sector_idx;

  struct fat_dentry *dentry;

  char *buf = fat_scratchbuf_alloc();
  if (!buf) {
    printf("Failed to allocate buffer for reading\n");
    return ERR_MEMALLOC;
  }

  const size_t dentries_per_sector = fat32_get_dentries_per_sector(fs);

  size_t last_dentry_idx = l->start_dentry_idx + l->num_dentries - 1;
  size_t dentry_rel_idx = last_dentry_idx % dentries_per_sector;

  dentry_sector_idx = l->start_sector_idx +
    last_dentry_idx / dentries_per_sector;

  err = fs->bdev->ops.read(fs->bdev, buf, dentry_sector_idx, 1);
  if (err != SUCCESS) {
    printf("Error reading sector %ld\n", dentry_sector_idx);
    goto out;
  }
  dentry = (struct fat_dentry *)(buf) + dentry_rel_idx;
  fat_dentry_set_cluster(dentry, first_cluster_idx);
  fat_dentry_set_file_size(dentry, file_size);

  err = fs->bdev->ops.write(fs->bdev, buf, dentry_sector_idx, 1);
  if (err != SUCCESS) {
    printf("Error writing to sector %ld, err:%d\n", dentry_sector_idx, err);
    goto out;
  }

out:
  fat_scratchbuf_free(buf);
  return err;
}

int fat32_resize(const struct fat32_fs *fs, const char *filepath,
  size_t new_size)
{
  int err;
  struct fat_dentry d;
  struct fat_dentry_loc l;
  size_t cluster_idx;
  size_t sect_idx_fat_start;
  size_t sect_idx_fat_end;
  size_t sect_idx;

  size_t num_clusters;
  size_t prev_last_cluster;

  size_t prev_fat_sector = 0;
  uint32_t *fat;
  const size_t fat_entries_per_sect = fat32_fat_entries_per_sector(fs);

  /* fat entry index */
  size_t fatent_idx;
  size_t file_size;

  char *buf = fat_scratchbuf_alloc();
  if (!buf) {
    printf("Failed to allocate buffer for reading\n");
    return ERR_MEMALLOC;
  }

  fat = (uint32_t *)buf;

  err = fat32_lookup(fs, filepath, &l, &d);
  if (err != SUCCESS) {
    printf("Path not found %s\n", filepath);
    return err;
  }

  file_size = fat32_get_file_size(&d);

  prev_last_cluster = fat32_dentry_get_cluster(&d);

  sect_idx_fat_start = fat32_fat_start_sector_index(fs, 0);
  sect_idx_fat_end = sect_idx_fat_start + fat32_get_sectors_per_fat(fs);
  num_clusters = fat_calc_num_clusters_for_data_size(fs, new_size);
  sect_idx = sect_idx_fat_start;

  for (;num_clusters && sect_idx < sect_idx_fat_end; sect_idx++) {
    err = fs->bdev->ops.read(fs->bdev, buf, sect_idx, 1);
    if (err != SUCCESS) {
      printf("Error reading sector %ld, err:%d\n", sect_idx, err);
      goto out;
    }

    fatent_idx = 0;
    bool sector_has_updates = false;
    for (; num_clusters && fatent_idx < fat_entries_per_sect; ++fatent_idx) {
      if (fat[fatent_idx])
        continue;

      sector_has_updates = true;
      fat[fatent_idx] = FAT32_ENTRY_END_OF_CHAIN_ALT;
      cluster_idx = (sect_idx - sect_idx_fat_start) * fat_entries_per_sect +
        fatent_idx;

      prev_fat_sector = sect_idx_fat_start +
        prev_last_cluster / fat_entries_per_sect;

      file_size += fat32_bytes_per_cluster(fs);

      if (file_size > new_size)
        file_size = new_size;

      /* update previous entry */
      if (!prev_last_cluster) {
        /*
         * Before this file did not own clusters, we need to update file's
         * dentry to point to newly created cluster chain of single cluster
         */
        fat_dentry_set_cluster(&d, cluster_idx);
        err = fat_dir_entry_mod(fs, &l, cluster_idx, file_size);
        if (err != SUCCESS) {
          printf("Failed to modify dentry, err: %d\r\n", err);
          goto out;
        }
      }
      else if (sect_idx == prev_fat_sector) {
        /*
         * File dentry already points to existing cluster chain, so we need
         * to update previous end of chain in fat table to point to new
         * entry
         * The question is - where is the entry, that was the previous end of
         * cluster chain? If it's in the same fat sector as the new end of
         * chain we can update the same sector that is cached in 'buf'
         * without the need to read / write to other sector
         */

        size_t prev_fatent = prev_last_cluster % fat_entries_per_sect;
        fat[prev_fatent] = cluster_idx;
      }
      else {
        err = fat_set_cluster_entry(fs, prev_last_cluster, cluster_idx);
        if (err != SUCCESS)
          goto out;
      }
      num_clusters--;
      prev_last_cluster = cluster_idx;
    }

    if (sector_has_updates) {
      err = fs->bdev->ops.write(fs->bdev, buf, sect_idx, 1);
      if (err != SUCCESS) {
        printf("Failed to write to block device, err:%d\n", err);
        goto out;
      }
    }
  }

  err = fat_dir_entry_mod(fs, &l, fat32_dentry_get_cluster(&d), file_size);
  if (err != SUCCESS)
    printf("Failed to final modify dentry, err: %d\r\n", err);

out:
  fat_scratchbuf_free(buf);
  return err;
}

int fat32_ls(const struct fat32_fs *fs, const char *dirpath)
{
  int err;
  uint32_t cluster;
  struct fat_dentry d;
  struct fat_dentry_loc loc;

  if (dirpath[0] == '/' && !dirpath[1])
    cluster = fat32_get_root_dir_cluster(fs);
  else {
    err = fat32_lookup(fs, dirpath, &loc, &d);
    if (err != SUCCESS) {
      printf("Path not found %s\n", dirpath);
      return err;
    }
    cluster = fat32_dentry_get_cluster(&d);
  }

  err = fat_iter_dentries(fs, cluster, fat32_dentry_print, 0);
  if (err) {
    printf("fat32_lookup: failed to iterate directory entries\r\n");
    return err;
  }

  return SUCCESS;
}

struct fat_file_iter_ctx {
  int err;
  fat_file_iter_cb on_sector_cb;
  void *cb_arg;
  size_t num_sectors_left;
};

static bool fat_iter_file_on_cluster(const struct fat32_fs *f,
  size_t cluster_idx, void *arg)
{
  bool should_continue = false;
  struct fat_file_iter_ctx *ctx = arg;
  size_t sect_idx = fat32_cluster_to_sector_idx(f, cluster_idx);

  size_t to_process = MIN(fat32_sectors_per_cluster(f),
    ctx->num_sectors_left);

  size_t end_sect_idx = sect_idx + to_process;
  char *buf = fat_scratchbuf_alloc();
  if (!buf) {
    printf("Failed to allocate buffer for reading\n");
    ctx->err = ERR_MEMALLOC;
    return should_continue;
  }

  while(sect_idx < end_sect_idx) {
    ctx->err = f->bdev->ops.read(f->bdev, buf, sect_idx, 1);
    if (ctx->err != SUCCESS) {
      printf("Error reading sector %ld, err:%d\n", sect_idx, ctx->err);
      goto out;
    }
    if (ctx->on_sector_cb(cluster_idx, sect_idx, ctx->cb_arg)
      == FAT_FILE_ITER_STOP)
      goto out;

    sect_idx++;
  }
  ctx->num_sectors_left -= to_process;

  if (!ctx->num_sectors_left)
    goto out;

  should_continue = true;

out:
  fat_scratchbuf_free(buf);
  return should_continue;
}

int fat_foreach_file_sect(const struct fat32_fs *fs,
  const struct fat_dentry *d,
  fat_file_iter_cb on_sector_cb,
  void *cb_arg)
{
  int err;
  const size_t sector_size = fat32_sector_size(fs);

  struct fat_file_iter_ctx iter_file_sect_ctx = {
    .err = SUCCESS,
    .on_sector_cb = on_sector_cb,
    .cb_arg = cb_arg,
    .num_sectors_left = (fat32_dentry_get_file_size(d) + sector_size - 1)
      / sector_size
  };

  err = fat_foreach_cluster(fs, fat32_dentry_get_cluster(d),
    fat_iter_file_on_cluster, &iter_file_sect_ctx);
  if (err != SUCCESS)
    printf("Failed to iterate clusters on file sectors iteration\n");

  return err;
}

static bool fat_iter_file_cluster_fn(const struct fat32_fs *fs,
  size_t cluster_idx, void *arg)
{
  printf(" %03lx\n", cluster_idx);
  return true;
}

int fat32_dump_file_cluster_chain(const struct fat32_fs *fs,
  const char *filename)
{
  int err;
  struct fat_dentry d, d_parent;
  uint32_t tmp;
  struct fat_dentry_loc loc;

  err = fat32_lookup(fs, filename, &loc, &d);
  if (err) {
    printf("fat32_dump_file_cluster_chain: failed to lookup file %s\r\n",
      filename);
    return err;
  }

  err = fat_foreach_cluster(fs, fat32_dentry_get_cluster(&d),
    fat_iter_file_cluster_fn, NULL);

  if (err != SUCCESS)
    printf("Failed to iterate clusters on file sectors iteration\n");

  return err;
}

static void fat32_dump_fat_sector(const struct fat32_fs *fs, size_t sect_idx,
  const uint32_t *data)
{
  size_t nents_per_sect = fat32_fat_entries_per_sector(fs);

  for (size_t i = 0; i < nents_per_sect; ++i)
    printf("%04lx.%08lx: %08x\n", sect_idx, nents_per_sect * sect_idx + i,
      data[i]);
}

void fat32_dump_fat(const struct fat32_fs *f, int fat_idx)
{
  int err;
  size_t sect_idx;
  size_t fat_start_idx = fat32_get_num_reserved_sectors(f) +
    fat_idx * fat32_get_sectors_per_fat(f);

  size_t fat_end_idx = fat_start_idx + fat32_get_sectors_per_fat(f);
  char *buf = fat_scratchbuf_alloc();
  if (!buf) {
    printf("Failed to allocate buffer for reading\n");
    return;
  }
  printf("Dumping FAT #%d, staring at sector %ld\n", fat_idx, fat_start_idx);
  for (sect_idx = fat_start_idx; sect_idx < fat_end_idx; ++sect_idx) {
    err = f->bdev->ops.read(f->bdev, buf, sect_idx, 1);
    if (err != SUCCESS) {
      printf("Block device reading not successful, err:%d\n", err);
      goto out;
    }

    fat32_dump_fat_sector(f, sect_idx - fat_start_idx, (const uint32_t *)buf);
  }
  printf("Finished dump of FAT #%d\n", fat_idx);
out:
  fat_scratchbuf_free(buf);
}

void fat32_dump_each_fat(const struct fat32_fs *f)
{
  size_t i;
  for (i = 0; i < fat32_get_num_fats(f); ++i)
    fat32_dump_fat(f, i);
}

struct fat_iter_cluster_read_ctx {
  size_t logic_sector_pos;
  size_t first_read_offset;
  size_t logic_sector_start;
  size_t read_size_bytes;
  uint8_t *dst;
  int err;
};

static bool fat_iter_cluster_read_cb(const struct fat32_fs *f,
  size_t cluster_idx, void *arg)
{
  bool should_continue = false;
  char *read_dst;
  size_t rel_sector_idx;
  struct fat_iter_cluster_read_ctx *ctx = arg;
  size_t physical_sector_idx;
  size_t io_size;
  const size_t sectors_per_clust = fat32_sectors_per_cluster(f);

  char *buf;

  /*
   * Skip the whole cluster if it's sectors do not include start_sector for the
   * IO op
   */
  if (ctx->logic_sector_pos + sectors_per_clust <= ctx->logic_sector_start) {
    ctx->logic_sector_pos += sectors_per_clust;
    return true;
  }

  buf = fat_scratchbuf_alloc();
  if (!buf) {
    printf("Failed to allocate buffer for reading\n");
    ctx->err = ERR_MEMALLOC;
    return should_continue;
  }

  /* We are in the right cluster. How far ahead is the sector we need? */
  rel_sector_idx = ctx->logic_sector_start - ctx->logic_sector_pos;

  /*
   * convert cluster pos to physical sector pos and add distance to sector
   * of interest
   */
  physical_sector_idx = fat32_cluster_to_sector_idx(f, cluster_idx) +
    rel_sector_idx;

  if (ctx->first_read_offset || ctx->read_size_bytes < fat32_sector_size(f)) {
    size_t sector_space = fat32_sector_size(f) - ctx->first_read_offset;
    io_size = MIN(sector_space, ctx->read_size_bytes);

    ctx->err = f->bdev->ops.read(f->bdev, buf, physical_sector_idx, 1);
    if (ctx->err != SUCCESS)
      goto out;

    memcpy(ctx->dst, buf + ctx->first_read_offset, io_size);
    ctx->first_read_offset = 0;
  }
  else {
    io_size = fat32_sector_size(f);
    ctx->err = f->bdev->ops.read(f->bdev, ctx->dst, physical_sector_idx, 1);
    if (ctx->err != SUCCESS)
      goto out;
  }
  ctx->read_size_bytes -= io_size;
  ctx->dst += io_size;

  if (ctx->read_size_bytes)
    should_continue = true;

out:
  fat_scratchbuf_free(buf);
  return should_continue;
}

int fat32_read(const struct fat32_fs *f, const struct fat_dentry *d,
  uint8_t *dst, size_t offset, size_t count)
{
  int err;
  size_t sect_sz = fat32_sector_size(f);
  size_t io_size;

  uint8_t *dst_orig = dst;
  size_t file_size = fat32_get_file_size(d);
  int cluster_idx;
  uint64_t data_sector_idx;
  size_t seek = 0;

  if (offset > file_size)
    return 0;

  if (file_size - offset < count)
    count = file_size - offset;

  struct fat_iter_cluster_read_ctx iter_cluster_read_ctx = {
    .logic_sector_pos = 0,
    .first_read_offset = offset % sect_sz,
    .logic_sector_start = offset / sect_sz,
    .read_size_bytes = count,
    .dst = dst
  };

  err = fat_foreach_cluster(f, fat32_dentry_get_cluster(d),
    fat_iter_cluster_read_cb, &iter_cluster_read_ctx);
  if (err == SUCCESS)
    err = iter_cluster_read_ctx.err;

  if (err != SUCCESS) {
    printf("Failed to iterate clusters on file read %d\n", err);
    return err;
  }

  return count - iter_cluster_read_ctx.read_size_bytes;
}

static int fat_find_child_by_name(const struct fat32_fs *fs,
  size_t parent_cluster_idx, const char *name, struct fat_dentry *out,
  struct fat_dentry_loc *out_loc)
{
  int err;
  struct fat_find_by_name_ctx ctx = {
    .fs = fs,
    .err = ERR_NOT_FOUND,
    .filename = name,
    .loc = out_loc
  };

  err = fat_iter_dentries(fs, parent_cluster_idx, fat_find_dentry_by_name,
    &ctx);

  if (err != SUCCESS) {
    printf("Failed to iterate directory");
    return err;
  }

  err = ctx.err;
  if (err == SUCCESS)
    memcpy(out, ctx.dentries + ctx.num_dentries - 1, sizeof(*out));

  return err;
}

typedef fat_iter_fat_sectors_result (*fat_iter_fat_sectors_cb)(
  const struct fat32_fs *fs, size_t sector_idx, void *arg);

static void fat_iter_fat_sectors(const struct fat32_fs *fs,
  fat_iter_fat_sectors_cb cb, void *arg)
{
  size_t i;
  size_t sector_idx_fat_start, sector_idx_fat_end;

  sector_idx_fat_start = fat32_get_num_reserved_sectors(fs);
  sector_idx_fat_end = sector_idx_fat_start + fat32_get_sectors_per_fat(fs);

  for (i = sector_idx_fat_start; i < sector_idx_fat_end; ++i) {
    if (cb(fs, i, arg) == FAT_ITER_FAT_SECTORS_STOP)
      break;
  }
}

/*
 * Cluster chain is implemented in FAT, which is a special table, describing
 * cluster allocation and linking status, not to be confused with FAT32 as the
 * name of the file system. In FAT, clusters are described in small entries,
 * values of which can be 0 for free cluster, 0xffffffff or 0xfffffff8 as a
 * busy cluster, that is not linked with next cluster or any other value,
 * which tells which cluster is next.
 * Say we have these values in one of the FAT's sectors:
 * 0xffffffff, 0x00000025, 0x00000004, 0xffffffff, 0x00000000
 * First 0xffffffff - means this cluster is allocated by some file or dir,
 * and it is the last cluster in a single linked list of a cluster chain
 * Next 0x00000025 - means this cluster is allocated and linked to some other
 * cluster with index 0x25,
 * Next we see 0x00000004 - means this cluster is linked to cluster with index
 * Then again 0xffffffff - cluster is allocated and is last in chain
 * Then we see 0, meaning this is a free cluster and could be allocated/claimed
 * some day by a file or a directory
 * fat_alloc_file_cluster grows cluster of chain by 1, exactly:
 * 1. follows the cluster chain staring from cluster 'cluster_idx'
 *    until end of cluster chain is reached (cluster marked with 0xffffffff)
 * 2. it then scans FAT by looking for the first cluster entry marked with 0
 * 3  then marks newly allocated cluster as end of cluster chain by writing
 *    0xffffffff to its entry.
 * 4. It then modifies last cluster entry, previously marked with 0xffffffff,
 *    by overwriting it with the index of this new end of chain
 */
struct fat_extend_cluster_chain_ctx {
  int err;
  size_t prev_last_cluster_idx;
  size_t new_last_cluster_idx;
  char *scratchbuf;
};

static fat_iter_fat_sectors_result fat_extend_cluster_chain_fn(
  const struct fat32_fs *fs, size_t sector_idx, void *arg)
{
  size_t entries_per_sector;
  size_t i;

  struct fat_extend_cluster_chain_ctx *ctx = arg;
  uint32_t *fat = (uint32_t *)ctx->scratchbuf;

  /* Scan FAT sector for next free cluster entry, that can be allocated */
  ctx->err = fs->bdev->ops.read(fs->bdev, ctx->scratchbuf, sector_idx, 1);
  if (ctx->err != SUCCESS) {
    printf("block device reading not successful, err:%d\n", ctx->err);
    return FAT_ITER_FAT_SECTORS_STOP;
  }

  entries_per_sector = fat32_sector_size(fs) / sizeof(uint32_t);

  i = 0;
  while(i < entries_per_sector && fat[i])
    i++;

  if (i == entries_per_sector)
    /* All cluster entries busy, move on to next FAT sector */
    return FAT_ITER_FAT_SECTORS_PROCEED;

  /* Found non busy cluster (entry value eq. 0) */
  fat[i] = FAT32_ENTRY_END_OF_CHAIN;
  ctx->err = fs->bdev->ops.write(fs->bdev, ctx->scratchbuf, sector_idx, 1);
  if (ctx->err != SUCCESS) {
    printf("Failed to write to block device, err:%d\n", ctx->err);
    return FAT_ITER_FAT_SECTORS_STOP;
  }
  /*
   * Convert entry in FAT idx to cluster idx 
   * sector_idx           - index of current sector relative to fs start
   * i                    - index of first free entry in current sector
   * start_fat_sector_idx - index of first FAT sector, relative fs start
   * rel_sector_idx       - index of current sector relative FAT.start.
   */
  size_t start_fat_sector_idx = fat32_fat_start_sector_index(fs, 0);
  size_t rel_sector_idx = sector_idx - start_fat_sector_idx;
  ctx->new_last_cluster_idx = rel_sector_idx * entries_per_sector + i;

  if (ctx->prev_last_cluster_idx) {
    size_t entries_per_fat_sector = fat32_entries_per_fat_sector(fs);
    size_t prev_last_cluster_entry_sector = start_fat_sector_idx +
      ctx->prev_last_cluster_idx / entries_per_fat_sector;

    size_t entry_idx = ctx->prev_last_cluster_idx % entries_per_fat_sector;

    ctx->err = fs->bdev->ops.read(fs->bdev, ctx->scratchbuf,
      prev_last_cluster_entry_sector, 1);
    if (ctx->err != SUCCESS) {
      printf("Failed to read from block device, err:%d\n", ctx->err);
      return FAT_ITER_FAT_SECTORS_STOP;
    }

    fat[entry_idx] = ctx->new_last_cluster_idx; 
    ctx->err = fs->bdev->ops.write(fs->bdev, ctx->scratchbuf,
      prev_last_cluster_entry_sector, 1);
    if (ctx->err != SUCCESS) {
      printf("Failed to write to block device, err:%d\n", ctx->err);
      return FAT_ITER_FAT_SECTORS_STOP;
    }
  }

  ctx->err = SUCCESS;
  return FAT_ITER_FAT_SECTORS_STOP;
}

static int fat_alloc_file_cluster(const struct fat32_fs *fs,
  size_t cluster_idx, size_t *new_cluster_idx)
{
  struct fat_extend_cluster_chain_ctx ctx;

  ctx.scratchbuf = fat_scratchbuf_alloc();
  if (!ctx.scratchbuf) {
    printf("Failed to allocate buffer for reading\n");
    return ERR_MEMALLOC;
  }
  ctx.err = ERR_NOT_FOUND;
  ctx.prev_last_cluster_idx = cluster_idx;
  ctx.new_last_cluster_idx = 0xffffffff;

  fat_iter_fat_sectors(fs, fat_extend_cluster_chain_fn, &ctx);
  if (ctx.err != SUCCESS || ctx.new_last_cluster_idx == 0xffffffff)
    printf("Failed to find free cluster in FAT\n");
  else
    *new_cluster_idx = ctx.new_last_cluster_idx;

  fat_scratchbuf_free(ctx.scratchbuf);
  return ctx.err;
}

static int fat_populate_dir_start_cluster(const struct fat32_fs *fs,
  size_t cluster_idx, size_t parent_cluster_idx)
{
  int err;

  struct fat_dentry *d;

  size_t sector_idx = fat32_cluster_to_sector_idx(fs, cluster_idx);

  char *buf = fat_scratchbuf_alloc();
  if (!buf) {
    printf("Failed to allocate buffer for reading\n");
    return ERR_MEMALLOC;
  }

  err = fs->bdev->ops.read(fs->bdev, buf, sector_idx, 1);
  if (err != SUCCESS) {
    printf("Error reading sector %ld\n", sector_idx);
    goto out;
  }

  d = (struct fat_dentry *)buf;
  /* Zero out 3 dentries at once, '.', '..', and end entry */
  memset(buf, 0, fat32_sector_size(fs));

  memset(d->filename_short, ' ', sizeof(d->filename_short));
  memset(d->extension, ' ', sizeof(d->extension));
  d->filename_short[0] = '.';
  d->file_attributes = FAT_FILE_ATTR_SUBDIRECTORY;

  d++;
  memset(d->filename_short, ' ', sizeof(d->filename_short));
  memset(d->extension, ' ', sizeof(d->extension));
  d->filename_short[0] = '.';
  d->filename_short[1] = '.';
  fat_dentry_set_cluster(d, parent_cluster_idx);

  d->file_attributes = FAT_FILE_ATTR_SUBDIRECTORY;

  err = fs->bdev->ops.write(fs->bdev, buf, sector_idx, 1);
  if (err != SUCCESS) {
    printf("Error writing to sector %ld, err:%d\n", sector_idx, err);
    goto out;
  }
  memset(buf, 0, fat32_sector_size(fs));
  for (size_t i = 1; i < 4; ++i) {
    err = fs->bdev->ops.write(fs->bdev, buf, sector_idx + i, 1);
    if (err != SUCCESS) {
      printf("Error writing to sector %ld, err:%d\n", sector_idx, err);
      goto out;
    }
  }

out:
  fat_scratchbuf_free(buf);
  return err;
}
/*
 * Allocate first cluster for new directory and populate it with '.' and '..',
 * Each directory must have those entries, so having a cluster allocated is
 * a must for a directory
 */
static int fat_alloc_dir_start_cluster(const struct fat32_fs *fs,
  size_t *cluster_idx, size_t parent_cluster_idx)
{
  int err = fat_alloc_file_cluster(fs, 0, cluster_idx);
  if (err == SUCCESS)
    err = fat_populate_dir_start_cluster(fs, *cluster_idx, parent_cluster_idx);

  return err;
}

static size_t fat_dir_get_free_entries(const struct fat32_fs *fs,
  const struct fat_dentry *dir_sector, size_t rq_num_entries)
{
  const struct fat_dentry *dentry = dir_sector;
  const struct fat_dentry *d_end = dentry + fat32_get_dentries_per_sector(fs);
  const struct fat_dentry *d_start = NULL;
  size_t result_nr_entries = 0;

  while(dentry < d_end) {
    if (fat_dentry_is_lfn_entry(dentry)) {
      if (!d_start)
        d_start = dentry;
      do {
        result_nr_entries++;
        dentry++;
      } while(fat_dentry_is_lfn_entry(dentry) && dentry < d_end);
    }

    if (dentry == d_end)
      break;

    if (fat32_dentry_allocatable(dentry)) {
      if (!d_start)
        d_start = dentry;
      result_nr_entries++;
      if (result_nr_entries >= rq_num_entries)
        break;
    }
    else {
      d_start = NULL;
      result_nr_entries = 0;
    }
    dentry++;
  }

  return result_nr_entries ? d_start - dir_sector : FAT_INVAL_DENTRY_IDX;
}

struct fat_get_free_dentries_ctx {
  /* input parameters */
  size_t num_dentries;

  /* Result */
  int err;
  struct fat_dentry_loc *loc;
};

static bool fat_alloc_dentries(const struct fat32_fs *fs, size_t cluster_idx,
  void *arg)
{
  int err;
  bool should_continue = false;

  struct fat_get_free_dentries_ctx *ctx = arg;
  size_t sect_idx, end_sect_idx;
  size_t first_free_entry_idx = FAT_INVAL_DENTRY_IDX;
  char *buf = fat_scratchbuf_alloc();
  if (!buf) {
    printf("Failed to allocate buffer for reading\n");
    ctx->err = ERR_MEMALLOC;
    return should_continue;
  }

  sect_idx = fat32_cluster_to_sector_idx(fs, cluster_idx);
  end_sect_idx = sect_idx + fat32_sectors_per_cluster(fs);

  while(sect_idx < end_sect_idx) {
    err = fs->bdev->ops.read(fs->bdev, buf, sect_idx, 1);
    if (err != SUCCESS) {
      ctx->err = err;
      printf("Error reading sector %ld\n", sect_idx);
      goto out;
    }

    first_free_entry_idx = fat_dir_get_free_entries(fs,
      (struct fat_dentry *)buf, ctx->num_dentries);

    if (first_free_entry_idx != FAT_INVAL_DENTRY_IDX)
      break;

    sect_idx++;
  }

  if (first_free_entry_idx != FAT_INVAL_DENTRY_IDX) {
    ctx->loc->start_sector_idx = sect_idx;
    ctx->loc->start_dentry_idx = first_free_entry_idx;
    ctx->loc->num_dentries = ctx->num_dentries;
    ctx->err = SUCCESS;
    goto out;
  }

  should_continue = true;
out:
  fat_scratchbuf_free(buf);
  return should_continue;
}

static void fat_lfn_fill_entries(struct vfat_lfn_entry *lfn_array,
  size_t num_lfns, const char *filename, uint8_t checksum)
{
  size_t lfn_idx = 0;
  struct vfat_lfn_entry *lfn;
  const char *filename_part;

  for (lfn_idx = 0; lfn_idx < num_lfns; ++lfn_idx) {
    lfn = &lfn_array[lfn_idx];
    fat_lfn_fill_name(lfn, filename + FAT_LFN_NAME_LEN * lfn_idx);
    lfn->seq_num = (num_lfns - lfn_idx) | (lfn_idx ? 0 : (1 << 6));
    lfn->file_attributes = FAT_FILE_ATTR_VFAT_LFN;
    lfn->file_type = 0;
    lfn->checksum = checksum;
    lfn->first_cluster = 0;
  }
}

static char to_uppercase(char c)
{
  if (c >= 'a' && c <= 'z')
    return c -= 0x20;
  return c;
}

static void fat_gen_8_3_shortname(struct fat_dentry *d, const char *longname)
{
  size_t len = strnlen(longname, 255);
  size_t len_no_ext = len;
  size_t dotpos = 0;
  size_t basename_len;
  size_t i;

  char *ptr;
  const char *ext = NULL;

  for (i = len; i > 0; --i) {
    if (i != len && longname[i - 1] == '.') {
      ext = longname + i;
      break;
    }
  }

  i = 0;
  if (ext) {
    len_no_ext = ext - 1 - longname;
    for (; i < 3 && ext[i]; ++i)
      d->extension[i] = to_uppercase(ext[i]);
  }

  for (;i < 3; ++i)
    d->extension[i] = ' ';

  basename_len = len_no_ext >= 9 ? 6 : len;
  for (i = 0; i < basename_len; ++i)
    d->filename_short[i] = to_uppercase(longname[i]);

  for (; i < sizeof(d->filename_short); ++i)
    d->filename_short[i] = ' ';

  if (basename_len == 6) {
    d->filename_short[6] = '~';
    d->filename_short[7] = '1';
  }

}

struct fat_dir_entry_mod_ctx {
  const char *filename;
  uint32_t first_cluster;
  uint32_t file_size;
};

fat_dir_iter_result fat_dir_entry_mod_fn(struct fat_dentry *dentries,
  size_t num_dentries, void *arg)
{
  char namebuf[128];
  struct fat_dir_entry_mod_ctx *ctx = arg;

  fat32_dentry_get_long_filename(namebuf, sizeof(namebuf), dentries,
    num_dentries);

  if (strcmp(namebuf, ctx->filename) == 0) {
    return FAT_DIR_ITER_STOP;
  }

  return FAT_DIR_ITER_CONTINUE;
}

static int fat_dir_create_entry2(const struct fat32_fs *fs,
  const struct fat_dentry_loc *l, const char *filename,
  bool is_dir, size_t parent_cluster_idx, struct fat_dentry *out_dentry)
{
  int err;

  struct fat_dentry *d, *main_dentry;
  struct vfat_lfn_entry *lfn_entry;
  const size_t dentries_per_sector = fat32_get_dentries_per_sector(fs);

  size_t cluster_idx;

  size_t sector_idx = l->start_sector_idx +
    l->start_dentry_idx / dentries_per_sector;

  size_t dentry_idx = l->start_dentry_idx % dentries_per_sector;

  char *buf;

  if (is_dir) {
    err = fat_alloc_dir_start_cluster(fs, &cluster_idx, parent_cluster_idx);
    if (err != SUCCESS) {
      printf("Failed to allocate start cluster for new dir %s, err: %d\r\n",
        filename, err);
      return ERR_GENERIC;
    }
  }
  else
    cluster_idx = 0;

  buf = fat_scratchbuf_alloc();
  if (!buf) {
    printf("Failed to allocate buffer for reading\n");
    return ERR_MEMALLOC;
  }

  err = fs->bdev->ops.read(fs->bdev, buf, sector_idx, 1);
  if (err < 0) {
    printf("Error reading sector %ld %d\n", sector_idx, err);
    goto out;
  }

  d = ((struct fat_dentry *)buf) + dentry_idx;
  main_dentry = d + l->num_dentries - 1;
  memset(main_dentry, 0, sizeof(*main_dentry));
  fat_gen_8_3_shortname(main_dentry, filename);

  main_dentry->file_attributes = is_dir ? FAT_FILE_ATTR_SUBDIRECTORY :
    FAT_FILE_ATTR_ARCHIVE;

  fat_dentry_set_cluster(main_dentry, cluster_idx);

  if (l->num_dentries > 1)
    fat_lfn_fill_entries((struct vfat_lfn_entry *)d, l->num_dentries - 1,
      filename, fat_calc_shortname_checksum(d + l->num_dentries - 1));

  err = fs->bdev->ops.write(fs->bdev, buf, sector_idx, 1);
  if (err != SUCCESS) {
    printf("Failed to write to block device, err:%d\n", err);
    goto out;
  }
  memcpy(out_dentry, main_dentry, sizeof(*main_dentry));
  err = SUCCESS;

out:
  fat_scratchbuf_free(buf);
  return err;
}

static inline int fat_dir_create_entry(const struct fat32_fs *fs,
  struct fat_dentry *parent,
  const struct fat_dentry_loc *parent_loc,
  const char *filename,
  bool is_dir,
  size_t file_size,
  size_t first_cluster_idx,
  struct fat_dentry *out_dentry,
  struct fat_dentry_loc *out_loc)
{
  int err;

  struct fat_get_free_dentries_ctx ctx = {
    .err = ERR_NOT_FOUND,
    .num_dentries = fat32_calc_num_dentries(filename),
    .loc = out_loc
  };

  size_t parent_cluster0;

  if (!ctx.num_dentries) {
    printf("Filepath too big to alloc lfn entries\n");
    return ERR_INVAL;
  }

  /* Ensure parent directory has cluster */
  parent_cluster0 = fat32_dentry_get_cluster(parent);

  if (!parent_cluster0) {
    err = fat_alloc_file_cluster(fs, 0, &parent_cluster0);
    if (err != SUCCESS) {
      printf("Failed to allocate next cluster\n");
      return err;
    }

    fat_dentry_set_cluster(parent, parent_cluster0);
    fat_dir_entry_mod(fs, parent_loc, parent_cluster0, 0);
  }

  err = fat_foreach_cluster(fs, parent_cluster0, fat_alloc_dentries, &ctx);
  if (err != SUCCESS) {
    printf("FAT cluster walk failed %d\n", err);
    return err;
  }

  err = ctx.err;
  if (err == ERR_NOT_FOUND) {
    size_t next_cluster_idx;
    err = fat_alloc_file_cluster(fs, parent_cluster0, &next_cluster_idx);
    if (err != SUCCESS) {
      printf("Failed to allocate next cluster\n");
      return err;
    }
    /* TODO: dir should be allocated with two entries '.' and '..' */
    ctx.loc->start_sector_idx = fat32_cluster_to_sector_idx(fs,
      next_cluster_idx);
    ctx.loc->start_dentry_idx = 0;
    ctx.loc->num_dentries = ctx.num_dentries;
  }
  else if (err != SUCCESS) {
    printf("Failed to find entry, %d\r\n", err);
    return err;
  }

  return fat_dir_create_entry2(fs, ctx.loc, filename, is_dir,
    parent_cluster0, out_dentry);
}

/*
 * fat_pathwalk_next - part of fat_pathwalk function, this function finds or
 * creates an entry with 'name' in 'parent' directory 
 * fs - file system info
 * parent       - dentry of a parent directory, that holds cluster address to
 *                list of directory entries, where to search filename
 * parent_loc   - location of parent dentry on a partition, in case if changes
 *                have to be done to directory meta
 * name         - name of entry in parent directory upon which to perform some
 *                action
 * is_dir       - should create or in case if exists - expected a directory
 * create       - create if missing or error
 * ok_if_exists - if function is called for file or dir creation if file exits
 *                this can be error or ok
 * out_dentry   - when file or dir is created in parent dir or just found this
 *                is where dentry meta is copied to, only the last entry, no
 *                long file name
 * out_loc      - new or found dentry location on partition.
 */
static int fat_pathwalk_next(const struct fat32_fs *fs,
  struct fat_dentry *parent,
  const struct fat_dentry_loc *parent_loc,
  const char *name,
  bool is_dir,
  bool create,
  bool ok_if_exists,
  struct fat_dentry *out_dentry,
  struct fat_dentry_loc *out_loc)
{
  int err = fat_find_child_by_name(fs, fat32_dentry_get_cluster(parent), name,
    out_dentry, out_loc);

  if (err == SUCCESS) {
    if (!ok_if_exists)
      return ERR_EXISTS;

    if (fat32_dentry_is_subdir(out_dentry) != is_dir) {
      printf("Pathwalk failed expected %s, got %s\n",
        is_dir ? "DIR" : "FILE", is_dir ? "FILE" : "DIR");
      return ERR_INVAL;
    }

    return SUCCESS;
  }

  if (err != ERR_NOT_FOUND) {
    printf("failed to get child entry %s from dir, err: %d\n", name, err); 
    return err;
  }

  if (create)
    err = fat_dir_create_entry(fs, parent, parent_loc, name, is_dir, 0, 0,
      out_dentry, out_loc);

  return err;
}

/*
 * Structure that describes what what type of file and in which directory
 * to create
 */
static int fat_pathwalk(const struct fat32_fs *fs, const char *filepath,
  bool create_midpath, bool create_last, bool ok_if_last_exists,
  bool last_is_dir)
{
  int err;
  const char *current_path;
  size_t len;
  struct fat_dentry parent = { 0 };
  char name[128];
  struct fat_dentry_loc parent_loc = { 0 };

  /* Test that input is valid */
  if (filepath[1] == 0) {
    printf("fat_pathwalk: invalid argument: filepath: %s\r\n", filepath);
    return ERR_INVAL;
  }

  fat_dentry_set_cluster(&parent, fat32_get_root_dir_cluster(fs));

  /*
   * Walk through the filepath argument to retreive:
   * - parent directory first data cluster index
   * - actual file name to create
   */
  current_path = filepath;

  while(*current_path) {
    if (*current_path != '/') {
      printf("Error while parsing path %s\n", filepath);
      return ERR_GENERIC;
    }

    current_path++;

    len = filepath_get_next_element(&current_path);

    if (len >= sizeof(name)) {
      printf("File name length is too big %s\n", current_path - len);
      return ERR_INVAL;
    }

    memcpy(name, current_path - len, len);
    name[len] = 0;

    bool is_dir;
    bool create;
    bool ok_if_exists;

    if (*current_path) {
      create = create_midpath;
      is_dir = true;
      ok_if_exists = true;
    }
    else {
      create = create_last;
      is_dir = last_is_dir;
      ok_if_exists = ok_if_last_exists;
    }

    err = fat_pathwalk_next(fs, &parent, &parent_loc, name, is_dir, create,
      ok_if_exists, &parent, &parent_loc);

    if (err != SUCCESS)
      return err;
  }

  return SUCCESS;
}


int fat32_create(const struct fat32_fs *fs, const char *filepath, bool is_dir,
  bool recursive)
{
  return fat_pathwalk(fs, filepath, recursive, true, false, is_dir);
}

struct fat_file_pos {
  size_t pos;
  size_t cluster_idx;
  size_t sector_idx;
  size_t sector_offset;
};

struct fat_file {
  const struct fat32_fs *fs;
  struct fat_dentry_loc loc;
  struct fat_file_pos pos;
  struct fat_dentry d_parent;
  struct fat_dentry d;
};

struct fat_file_pos_advance_ctx {
  int err;
  size_t target_offset;
  struct fat_file_pos *pos;
};

static bool fat_file_pos_next_cluster(const struct fat32_fs *fs,
  size_t cluster_idx, void *arg)
{
  struct fat_file_pos_advance_ctx *ctx = arg;
  size_t byte_offset;

  ctx->pos->cluster_idx = cluster_idx;

  if (ctx->target_offset < ctx->pos->pos) {
    ctx->pos->pos += fat32_bytes_per_cluster(fs);
    return true;
  }

  byte_offset = ctx->target_offset - ctx->pos->pos;

  const size_t sector_size = fat32_sector_size(fs);
  ctx->err = SUCCESS;
  ctx->pos->sector_idx = byte_offset / sector_size;
  ctx->pos->sector_offset = byte_offset % sector_size;
  return false;
}

static int fat_file_pos_advance(struct fat_file *f, size_t offset)
{
  int err;

  struct fat_file_pos_advance_ctx ctx = {
    .err = ERR_OUT_OF_RANGE,
    .target_offset = offset,
    .pos = &f->pos
  };

  err = fat_foreach_cluster(f->fs, fat32_dentry_get_cluster(&f->d),
    fat_file_pos_next_cluster, &ctx);

  if (err == SUCCESS && ctx.err != SUCCESS)
    err = ctx.err;

  if (err != SUCCESS && err != ERR_OUT_OF_RANGE)
    printf("fat_file_pos_advance: failed to iterate sectors %d\r\n", err);

  return err;
}
static int fat32_write_one_sect(const struct fat32_fs *fs,
  const struct fat_file_pos *pos, const uint8_t *src, size_t max_io_size,
  size_t *io_size)
{
  int err;
  char *buf;
  const size_t sect_sz = fat32_sector_size(fs);
  const size_t io_sector = fat32_cluster_to_sector_idx(fs, pos->cluster_idx) +
    pos->sector_idx;

  const size_t io_sz = MIN(sect_sz - pos->sector_offset, max_io_size);
  if (io_sz == sect_sz) {
    /*
     * Simle case - just write complete sector full of new data, nothing to
     * read first
     */
    err = fs->bdev->ops.write(fs->bdev, src, io_sector, 1);
    if (err < 0)
      printf("fat32_write: error write sector %ld, err: %d\r\n", io_sector,
        err);
    return err;
  }

  /*
   * Partial sector write case, writing at non-zero offset or not until the
   * end of sector, first need to read sector to buf, then copy new data at
   * proper offset ot buf, then write the buf back to sector
   */
  buf = fat_scratchbuf_alloc();
  if (!buf) {
    printf("fat32_write: failed to alloc scratchbuf\r\n");
    return ERR_MEMALLOC;
  }

  err = fs->bdev->ops.read(fs->bdev, buf, io_sector, 1);
  if (err < 0) {
    printf("fat32_write: error reading sector %ld, err: %d\r\n", io_sector,
      err);
  }

  memcpy(buf + pos->sector_offset, src, io_sz);
  err = fs->bdev->ops.write(fs->bdev, buf, io_sector, 1);
  if (err < 0)
    printf("fat32_write: error write sector %ld, err: %d\r\n", io_sector, err);

out:
  fat_scratchbuf_free(buf);
  return err;
}

static void fat_file_pos_init(struct fat_file_pos *pos)
{
  memset(pos, 0, sizeof(*pos));
}

int fat32_write(const struct fat32_fs *fs, const char *filepath, size_t offset,
    size_t size, const void *data)
{
  int err;
  struct fat_file f = { .fs = fs };
  size_t sect_sz = fat32_sector_size(fs);
  size_t size_left = size;
  size_t logic_sector_idx;
  size_t sector_byte_offset;
  size_t new_cluster_idx;
  size_t io_size;
  const uint8_t *src = data;
  err = fat32_lookup(fs, filepath, &f.loc, &f.d);
  if (err != SUCCESS) {
    printf("fat32_write: failed to lookup file %s, %d\r\n",
      filepath, err);
    return err;
  }

  fat_file_pos_init(&f.pos);
  err = fat_file_pos_advance(&f, offset);
  if (err != SUCCESS && err != ERR_OUT_OF_RANGE) {
    printf("fat32_write: seek failed %d\r\n", err);
    return err;
  }

  while (err == ERR_OUT_OF_RANGE) {
    err = fat_alloc_file_cluster(f.fs, f.pos.cluster_idx,
      &new_cluster_idx);

    if (err != SUCCESS) {
      printf("Failed to allocate next cluster\n");
      return err;
    }

    f.d.file_size += fat32_bytes_per_cluster(fs);
    if (!fat32_dentry_get_cluster(&f.d)) {
      fat_dentry_set_cluster(&f.d, new_cluster_idx);
      fat_dir_entry_mod(fs, &f.loc, new_cluster_idx, f.d.file_size);
    }

    err = fat_file_pos_advance(&f, offset);
    if (err != SUCCESS && err != ERR_OUT_OF_RANGE) {
      printf("fat32_write: seek failed %d\r\n", err);
      return err;
    }
  }

  err = fat32_write_one_sect(fs, &f.pos, src, size_left, &io_size);
  if (err != SUCCESS)
    return err;

  err = fat_file_pos_advance(&f, io_size);
  if (err != SUCCESS && err != ERR_OUT_OF_RANGE) {
    printf("fat32_write: seek failed %d\r\n", err);
    return err;
  }

  return err;
}

void fat32_init(void)
{
  fat_scratchbuf_mask = 0;
}

#include <fs/fat32.h>
#include <fs/mbr.h>
#include <fcntl.h>
#include <unistd.h>
#include <errcode.h>


static int sdcard_fd = -1;

static char readbuf[512];

static int (fn_read)(struct block_device *b, char *dst,
  size_t start_sector, size_t num_sectors)
{
  off_t offset = start_sector * b->sector_size;
  size_t count = num_sectors * b->sector_size;
  ssize_t ret = pread(sdcard_fd, dst, count, offset);
  if (ret < 0)
    return -1;

  if ((size_t)ret != count)
    printf("short read\n");

  return 0;
}

static int (fn_write)(struct block_device *b, const void *src,
  size_t start_sector, size_t num_sectors)
{
  off_t offset = start_sector * b->sector_size;
  size_t count = num_sectors * b->sector_size;
  ssize_t ret = pwrite(sdcard_fd, src, count, offset);
  if (ret < 0)
    return -1;

  if ((size_t)ret != count)
    printf("short write\n");
  return 0;
}

static struct block_device my_block_device = {
  .ops = {
    .read = fn_read,
    .write = fn_write,
  },
  .sector_size = 512
};

static bool test_copy_file(const struct fat32_fs *f, const char *filepath,
  const char *destpath)
{
  int err;
  ssize_t io_ret;
  size_t file_size;
  size_t read_progress;
  int fd, i;
  size_t rq_count;
  struct fat_dentry file_entry;
  struct fat_dentry_loc loc;

  printf("Test copying file %s\n", filepath);
  printf("Dumping cluster chain for file %s\n", filepath);
  fat32_dump_file_cluster_chain(f, filepath);

  err = fat32_lookup(f, filepath, &loc, &file_entry);
  if (err != SUCCESS) {
    printf("Loopup failed for %s, err: %d\n", filepath, err);
    return false;
  }

  file_size = fat32_get_file_size(&file_entry);
  printf("Copying file %s to %s, size: %ld\n", filepath, destpath, file_size);

  fd = open(destpath, O_CREAT|O_WRONLY, 0755);
  if (fd == -1) {
    perror("Failed to open file for writing");
    return false;
  }

  if (ftruncate(fd, 0) == -1) {
    perror("Failed to truncate file");
    return false;
  }

  read_progress = 0;
  i = 0;
  while (read_progress < file_size) {
    rq_count = MIN(file_size - read_progress, sizeof(readbuf));

    printf("reading io:%d, %ld(0x%06lx)/%ld\n", i, read_progress,
      read_progress, rq_count);

    i++;

    io_ret = fat32_read(f, &file_entry, readbuf, read_progress, rq_count);
    if (io_ret != rq_count) {
      printf("Failed\n");
      return false;
    }

    if (io_ret < 0 || io_ret == 0) {
      printf("fat32_read failed: %ld\n", io_ret);
      return false;
    }

    if (write(fd, readbuf, io_ret) != io_ret) {
      perror("Failed to write bytes to file");
      return false;
    }

    read_progress += io_ret;
  }
  return true;
}

char buf[512];

static int raw_disk_open(const char *disk_dev, int *out_fd)
{
  int fd = open(disk_dev, O_RDWR);
  if (fd == -1) {
    perror("Failed to open disk device");
    return -1;
  }
  *out_fd = fd;
  return 0;
}
char lbuf[230400];

int main()
{
  int err;
  struct fat32_fs fs;
  struct fat_dentry_loc l;
  struct fat_dentry d;
  int fd;

  sdcard_fd = open("/dev/sdb2", O_RDWR);
  if (sdcard_fd == -1) {
    perror("Failed to open sdcard device");
    return -1;
  }

#if 0
  if (raw_disk_open("/dev/sda", &fd) == -1)
    return -1;
#endif

  struct mbr mbr;
  read(fd, &mbr, sizeof(mbr));

  fat32_fs_open(&my_block_device, &fs);
  err = fat32_create(&fs, "/test", true, true);
  int fdr = open("../a.img", O_RDONLY);
  read(fdr, lbuf, sizeof(lbuf));

  if (err != SUCCESS && err != ERR_EXISTS) {
    printf("create failed\n");
    return -1;
  }
  err = fat32_create(&fs, "/test/img_0000.img", false, false);
  if (err != SUCCESS) {
    printf("create failed\n");
    return -1;
  }

  err = fat32_resize(&fs, "/test/img_0000.img", sizeof(lbuf));
  if (err != SUCCESS) {
    printf("resize failed\n");
    return -1;
  }

  err = fat32_write(&fs, "/test/img_0000.img", 0, sizeof(lbuf), lbuf);
  if (err != SUCCESS) {
    printf("Failed to write file\r\n");
    return -1;
  }

  fat32_ls(&fs, "/test");
  return 0;
  // fat32_dump_file_cluster_chain(&fs, "/test/data");
#if 1
//   fat32_dump_each_fat(&fs);
  err = fat32_create(&fs, "/test/data", false, true);
  if (err != SUCCESS && err != ERR_EXISTS) {
    printf("create failed\n");
    return -1;
  }

  fat32_resize(&fs, "/test/data", 512 * 4 * 200);
  memset(buf, 0x31, 512);
  for (int i = 0; i < 4 * 200; ++i)
    fat32_write(&fs, "/test/data", i * 512, 512, buf);
#endif
//  fat32_write(&fs, "/test/aa", 0, 512, buf);
//  fat32_lookup(&fs, "/test", &l, &d);
//  fat32_ls(&fs, "/test");


//  fat32_write(&fs, "/test/langgile2", 0, 512, buf);
 // fat32_ls(&fs, "/overlays");
 // test_copy_file(&fs, "/COPYING.linux", "/tmp/COPYING.linux");
 // test_copy_file(&fs, "/bootcode.bin", "/tmp/bootcode.bin");
 // fat32_create(&fs, "/test/newfile98");

  return 0;
}

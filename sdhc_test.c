#include "sdhc_test.h"
#include <errcode.h>
#include <os_api.h>
#include <cpu.h>
#include <list.h>
#include <list_fifo.h>
#include <logger.h>
#include <bcm2835/bcm_sdhost.h>
#include <string.h>
#include <kmalloc.h>
#include <printf.h>
#include <common.h>
#include <log.h>

extern struct sdhc_cmd_stat bcm_sdhost_cmd_stats;

static char sdhc_testbuf[512 * 512] = { 0 };

static struct write_stream_buf iobufs[128];

static inline void hexdump(const uint8_t *buffer, uint32_t size)
{
  uint32_t i, j;

  for (i = 0; i < size; i += 16) {
    for (j = 0; j < 16 && i + j < size; j++)
      os_log("%02x ", buffer[i + j]);
    os_log("\r\n");
  }
}

static OPTIMIZED void sdhc_dump_io_stats(int io_idx, uint64_t io_start,
  uint64_t io_end)
{
  const struct sdhc_cmd_stat *const s = &bcm_sdhost_cmd_stats;
  os_log("%ld %ld %ld %ld %ld %ld %ld %ld\r\n",
    io_start,
    s->multiblock_start_time,
    s->multiblock_end_time,
    s->wait_rdy_time,
    s->cmd_start_time,
    s->cmd_end_time,
    s->data_end_time,
    io_end);
}

void OPTIMIZED sdhc_perf_measure(struct block_device *bdev)
{
  int err;
  uint64_t io_start_time;
  uint64_t io_end_time;

  os_wait_ms(100);
  for (int i = 0; i < 256; ++i) {
    for (int j = 0; j < 512; ++j) {
      sdhc_testbuf[i * 512 + j] = (uint8_t)(i - j);
    }
  }
#if 0
  memset(sdhc_testbuf, 0x11, 512);
  memset(sdhc_testbuf + 512, 0x22, 512);
  memset(sdhc_testbuf + 1024, 0x44, 1024);
  memset(sdhc_testbuf + 2048, 0x55, sizeof(sdhc_testbuf) - 2048);
#endif
  dcache_invalidate(sdhc_testbuf, sizeof(sdhc_testbuf));

  /* buffer size = 512 * 256 = 128 * 1024 = 128K */
  /* Total write = 100Mb = 100 * 1024 * 1024 */

  for (int i = 0; i < 400; ++i) {
    io_start_time = arm_timer_get_count();
    err = bdev->ops.write(bdev, (uint8_t *)sdhc_testbuf, i * 512, 512);
    if (err) {
      os_log("failed to write to SD partition\r\n");
      while(1)
        asm volatile("wfe");
    }

    io_end_time = arm_timer_get_count();
    sdhc_dump_io_stats(i, io_start_time, io_end_time);
  }
}

static void write_stream_init(struct write_stream_buf *b, void *data,
  uint32_t buf_size)
{
  memset(b, 0, sizeof(*b));
  INIT_LIST_HEAD(&b->list);

  b->paddr = (uint32_t)(uint64_t)data | 0xc0000000;
  b->size = buf_size;
  b->io_size = 0;
  b->io_offset = 0;
}

static __attribute__((unused)) void sdhc_test_write_stream(struct block_device *bdev)
{
  int err;
  char *buf;
  const uint32_t initial_sector = 1056768;
  struct write_stream_buf *b;
  struct write_stream_buf *tmp;
  uint32_t io_tasks[] = {
    28,
    1575,
    655
  };

  buf = (char *)dma_alloc(512 * 128, 0);

  memset(buf, 0x77, 512 * 128);
  memset(buf +    0, 0x11, 512);
  memset(buf +  512, 0x22, 512);
  memset(buf + 1024, 0x33, 512);
  memset(buf + 1536, 0x44, 512);
  hexdump((uint8_t *)buf, 512);

  LIST_HEAD(bufs);

  err = bdev->ops.write_stream_open(bdev, initial_sector);
  if (err != SUCCESS)
    goto error;

  for (int i = 0; i < ARRAY_SIZE(io_tasks); ++i) {
    b = &iobufs[i];
    write_stream_init(b, buf, io_tasks[i]);
    list_fifo_push_tail(&bufs, &b->list);
    os_log("writing %d bytes \r\n", b->size);
    err = bdev->ops.write_stream_push(bdev, &bufs);
    list_for_each_entry_safe(b, tmp, &bufs, list) {
      os_log("buf with size %d released\r\n", b->size);
      list_del(&b->list);
    }
    os_log("write_stream_push returned %d\r\n", err);
    if (err != SUCCESS)
      goto error;
  }

  os_log("sdhc_test_write_stream done\r\n");
  
  while(1) {
    asm volatile("wfi");
  }

error:
  os_log("sdhc_test_write_stream failed\r\n");
  while(1) {
    asm volatile("wfi");
  }
}

static bool sdhc_self_test_read(struct sdhc *s)
{
  int err;
  int i;
  const uint32_t num_sectors = 2;
  const uint64_t from_sector = 8192;

  os_log("Running self-test read\r\n");

  for (i = 0; i < 4; ++i) {
    os_log("Reading %d blocks, starting from block #%ld, iteration #%d\r\n",
      num_sectors, from_sector, i);

    memset(sdhc_testbuf, 0, sizeof(sdhc_testbuf));
    err = sdhc_read(s, (uint8_t *)sdhc_testbuf, from_sector, num_sectors);
    if (err) {
      os_log("failed to read from SD\r\n");
      return false;
    }
    os_log("Reading done\r\n");
  
    hexdump((uint8_t *)sdhc_testbuf, 32);
  }
  os_log("Completed self-test read\r\n");
  return true;
}

static bool sdhc_self_test_write(struct sdhc *s, uint32_t from_block)
{
  int i;
  int err;
  os_log("Running self-test write\r\n");

  for (i = 0; i < 256; ++i) {
    sdhc_testbuf[i * 4 + 0] = i & 0xff;
    sdhc_testbuf[i * 4 + 1] = i / 4;
    sdhc_testbuf[i * 4 + 2] = 0xd6;
    sdhc_testbuf[i * 4 + 3] = 0xd7;
  }
  
  err = sdhc_write(s, (uint8_t *)sdhc_testbuf, from_block, 2);
  if (err) {
    os_log("failed to write to SD\r\n");
    return false;
  }

  os_log("Self-test write done\r\n");
  return true;
}

static bool sdhc_run_self_test_in_mode(struct sdhc *s, sdhc_io_mode_t mode,
  uint32_t test_partition_offset, bool cache_mem_used)
{
  int err;

  os_log("Running sdhc self-test in mode:%d (%s), cachemem:%s\r\n", mode,
    sdhc_io_mode_to_str(mode), cache_mem_used ? "yes" : "no");

  err = sdhc_set_io_mode(s, mode, cache_mem_used);

  if (err != SUCCESS) {
    os_log("Failed to set sdhc io mode %s\r\n", sdhc_io_mode_to_str(mode));
    return false;
  }

  if (!sdhc_self_test_read(s)) {
    os_log("SDHC self-test failed on read test. mode:%s\r\n",
      sdhc_io_mode_to_str(mode));
    return false;
  }

  if (!sdhc_self_test_write(s, test_partition_offset)) {
    os_log("SDHC self-test failed on write test, mode:%s\r\n",
      sdhc_io_mode_to_str(mode));
    return false;
  }

  os_log("Completed sdhc self-test mode:%d (%s)\r\n", mode,
    sdhc_io_mode_to_str(mode));
  return true;
}

bool sdhc_run_self_test(struct sdhc *s, struct block_device *bdev)
{
  bool passed;
  const uint32_t test_partition_offset = 1056768;
  const bool cache_mem_used = true;

  os_log("Starting SDHC self-test\r\n");
  passed = sdhc_run_self_test_in_mode(s, SDHC_IO_MODE_IT_DMA,
    test_partition_offset, cache_mem_used);
  // sdhc_test_write_stream(bdev);
  os_log("SDHC self-test %s\r\n", passed ? "PASSED" : "FAILED");
  return true;
}

#include <gpio.h>
#include <cpu.h>
#include <block_device.h>
#include <kmalloc.h>
#include <bcm2835/bcm2835_systimer.h>
#include <bcm2835/bcm2835_emmc.h>
#include <bcm2835/bcm2835_pll.h>
#include <bcm2835/bcm_sdhost.h>
#include <bcm2835_dma.h>
#include <common.h>
#include <debug_led.h>
#include "uart_pl011.h"
#include <stringlib.h>
#include <mbox.h>
#include <mbox_props.h>
#include <irq.h>
#include <sched.h>
#include <task.h>
#include <os_api.h>
#include <mmu.h>
#include <printf.h>
#include <atomic.h>
#include <sections.h>
#include <vchiq.h>
#include <ili9341.h>
#include <errcode.h>
#include <fs/fs.h>
#include <fs/fat32.h>
#include <logger.h>
#include <list_fifo.h>

static struct block_device *fs_blockdev;
static char sdhc_testbuf[512 * 512] = { 0 };
static struct sdhc sdhc;
struct block_device bdev_sdcard;
struct block_device *bdev_partition;
extern struct sdhc_cmd_stat bcm_sdhost_cmd_stats;

volatile char buf1[1024];
volatile char buf2[1024];

volatile int myvar = 10;

EXCEPTION void fiq_handler(void)
{
}

EXCEPTION void serror_handler(void)
{
}

void print_mbox_props(void)
{
  uint64_t val64;
  uint32_t val, val2, i;
  unsigned clock_rate;
  char buf[6];
  mbox_get_firmware_rev(&val);
  printf("firmware rev:    %08x\r\n", val);
  mbox_get_board_model(&val);
  printf("board model:     %08x\r\n", val);
  mbox_get_board_rev(&val);
  printf("board rev:       %08x\r\n", val);
  mbox_get_board_serial(&val64);
  printf("board serial:    %08x\r\n", val64);
  mbox_get_mac_addr(&buf[0], &buf[7]);
  printf("mac address:     %02x:%02x:%02x:%02x:%02x:%02x\r\n",
    (int)(buf[0]),
    (int)(buf[1]),
    (int)(buf[2]),
    (int)(buf[3]),
    (int)(buf[4]),
    (int)(buf[5]),
    (int)(buf[6]));

  mbox_get_arm_memory(&val, &val2);
  printf("arm memory base: %08x\r\n", val);
  printf("arm memory size: %08x\r\n", val2);

  mbox_get_vc_memory(&val, &val2);
  printf("vc memory base:  %08x\r\n", val);
  printf("vc memory size:  %08x\r\n", val2);
  for (i = 0; i < 8; ++i) {
    mbox_get_clock_rate(i, &clock_rate);
    //   printf("failed to get clock rate for clock_id %d\n", i);
    printf("clock %d rate: %08x (%d KHz)\r\n", i, clock_rate, clock_rate / 1000);
  }

#define GET_DEVICE_POWER_STATE(x)\
  mbox_get_power_state(MBOX_DEVICE_ID_ ## x, (uint32_t*)&val, (uint32_t*)&val2);;\
  printf("power_state: " #x ": on:%d,exists:%d\r\n", val, val2);

  GET_DEVICE_POWER_STATE(SD);
  GET_DEVICE_POWER_STATE(UART0);
  GET_DEVICE_POWER_STATE(UART1);
  GET_DEVICE_POWER_STATE(USB);
  GET_DEVICE_POWER_STATE(I2C0);
  GET_DEVICE_POWER_STATE(I2C1 );
  GET_DEVICE_POWER_STATE(I2C2);
  GET_DEVICE_POWER_STATE(SPI);
  GET_DEVICE_POWER_STATE(CCP2TX);
}

static inline void hexdump(const uint8_t *buffer, uint32_t size)
{
  uint32_t i, j;

  for (i = 0; i < size; i += 16) {
    for (j = 0; j < 16 && i + j < size; j++)
      printf("%02x ", buffer[i + j]);
    printf("\r\n");
  }
}

struct write_stream_buf iobufs[128];

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

static void sdhc_test_write_stream(struct sdhc *s)
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
  hexdump(buf, 512);

  LIST_HEAD(bufs);

  err = bdev_sdcard.ops.write_stream_open(&bdev_sdcard, initial_sector);
  if (err != SUCCESS)
    goto error;

  for (int i = 0; i < ARRAY_SIZE(io_tasks); ++i) {
    b = &iobufs[i];
    write_stream_init(b, buf, io_tasks[i]);
    list_fifo_push_tail(&bufs, &b->list);
    printf("writing %d bytes \r\n", b->size);
    err = bdev_sdcard.ops.write_stream_push(&bdev_sdcard, &bufs);
    list_for_each_entry_safe(b, tmp, &bufs, list) {
      printf("buf with size %d released\r\n", b->size);
      list_del(&b->list);
    }
    printf("write_stream_push returned %d\r\n", err);
    if (err != SUCCESS)
      goto error;
  }

  printf("sdhc_test_write_stream done\r\n");
  
  while(1) {
    asm volatile("wfi");
  }

error:
  printf("sdhc_test_write_stream failed\r\n");
  while(1) {
    asm volatile("wfi");
  }
}

static int sdhc_test_io(struct sdhc *s, bool skip_it)
{
  int i;
  int err;

  struct {
    sdhc_io_mode_t mode;
    const char *name;
  } test_modes[] = {
    // { SDHC_IO_MODE_BLOCKING_PIO, "BLOCKING_PIO" },
    // { SDHC_IO_MODE_BLOCKING_DMA, "BLOCKING_DMA" },
    { SDHC_IO_MODE_IT_DMA, "IT_DMA" }
  };

  for (i = 0; i < ARRAY_SIZE(test_modes); ++i) {
    if (skip_it && test_modes[i].mode == SDHC_IO_MODE_IT_DMA)
      continue;

    err = sdhc_set_io_mode(s, test_modes[i].mode, true);
    if (err != SUCCESS) {
      os_log("Failed to set sdhc io mode %s\r\n", test_modes[i].name);
      return err;
    }

    for (int j = 0; j < 4; ++j) {
      os_log("Testing SDHC with mode %s, iter%d\r\n", test_modes[i].name, j);
      memset(sdhc_testbuf, 0, sizeof(sdhc_testbuf));
      err = sdhc_read(s, sdhc_testbuf, 8196, 2);
      if (err) {
        os_log("failed to read from SD\r\n");
        return err;
      }
  
      hexdump(sdhc_testbuf, 32);
    }

    for (int i = 0; i < 256; ++i) {
      sdhc_testbuf[i * 4 + 0] = i & 0xff;
      sdhc_testbuf[i * 4 + 1] = i / 4;
      sdhc_testbuf[i * 4 + 2] = 0xd6;
      sdhc_testbuf[i * 4 + 3] = 0xd7;
    }
  
    err = sdhc_write(&sdhc, sdhc_testbuf, 1056768, 2);
    if (err) {
      os_log("failed to write to SD\r\n");
      while(1)
        asm volatile("wfe");
    }

    os_log("SDHC test mode %s done\r\n", test_modes[i].name);
  }

  os_log("sdhc_test_io DONE\r\n");
  return SUCCESS;
}

static void kernel_init(void)
{
  int err;

  uart_pl011_init(115200);
  clear_reboot_request();
  kmalloc_init();
  dma_memory_init();
  print_mbox_props();
  bcm2835_report_clocks();
  irq_init();
  bcm2835_systimer_init();
  irq_disable();
  mem_allocator_init();
  scheduler_init();
  debug_led_init();
  bcm2835_dma_init();
  err = logger_init();
  if (err != SUCCESS) {
    printf("Failed to init logger, err: %d\r\n", err);
    goto out;
  }

out:
  if (err != SUCCESS) {
    printf("Going to err loop\r\n");
    while(1)
      asm volatile("wfe");
  }
}

atomic_t test_atomic;

struct event test_ev;

char *readbuf;

extern uint64_t sdhost_ts_cmd_start;

static OPTIMIZED void sdhc_dump_io_stats(int io_idx, uint64_t io_start, uint64_t io_end)
{
  const struct sdhc_cmd_stat *const s = &bcm_sdhost_cmd_stats;
#if 0
  os_log("wr#%d: %d %d %d %d %d\r\n", io_idx,
    (uint32_t)(io_end - io_start),
    (uint32_t)(s->cmd_start_time - s->wait_rdy_time),
    (uint32_t)(s->cmd_end_time - s->cmd_start_time),
    (uint32_t)(s->data_end_time - s->cmd_end_time),
    (uint32_t)(s->multiblock_end_time - s->multiblock_start_time));
#endif

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

static OPTIMIZED void sdhc_measure(void)
{
  int err;
  int i;
  uint64_t io_start_time;
  uint64_t io_end_time;

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
    // os_wait_ms(2);

    io_start_time = arm_timer_get_count();
    err = bdev_partition->ops.write(bdev_partition, sdhc_testbuf, i * 512, 512);
    if (err) {
      os_log("failed to write to SD partition\r\n");
      while(1)
        asm volatile("wfe");
    }

    io_end_time = arm_timer_get_count();
    sdhc_dump_io_stats(i, io_start_time, io_end_time);
  }
}

static void app_main(void)
{
  int err;
  os_log("Application main\r\n");

  err = sdhc_init(&bdev_sdcard, &sdhc, &bcm_sdhost_ops);
  if (err != SUCCESS) {
    printf("Failed to init sdhc, %d\r\n", err);
    goto out;
  }
  os_log("SDHC intialized\r\n");
  blockdev_scheduler_init();

#if 1
  err = sdhc_test_io(&sdhc, false);
  if (err != SUCCESS)
    goto out;
#endif

  err = sdhc_set_io_mode(&sdhc, SDHC_IO_MODE_IT_DMA, false);
  if (err != SUCCESS) {
    os_log("Failed to set sdhc io mode IT_DMA\r\n");
    goto out;
  }

#if 0
  os_wait_ms(500);
  sdhc_test_write_stream(&sdhc);
#endif

  err = fs_init(&bdev_sdcard, &bdev_partition);
  if (err != SUCCESS) {
    os_log("Failed to init fs block device, err: %d\r\n", err);
    goto out;
  }

  os_wait_ms(100);
#if 0
  sdhc_measure();

  while(1) {
    asm volatile ("wfi");
  }

#endif
  err = ili9341_init();
  if (err != SUCCESS) {
    printf("Failed to init ili9341 display, err: %d\r\n", err);
    goto out;
  }

  fs_dump_partition(bdev_partition);
  vchiq_init(bdev_partition);
out:
  os_exit_current_task();
}

static void kernel_run(void)
{
  struct task *t;
#if 0
  mmu_print_va(0x0000000001ff1000, 1);
  mmu_print_va(0xffff000001ff0000, 1);
  mmu_print_va(0xffff000001ff1000, 1);
  mmu_print_va(0xffff00000201c000, 1);
#endif

  t = task_create(app_main, "app_main");
  sched_run_task_isr(t);
  t = task_create(blockdev_scheduler_fn, "block-sched");
  sched_run_task_isr(t);
  scheduler_start();
  panic();
}

void SECTION(".text") main(void)
{
  kernel_init();
  kernel_run();
}

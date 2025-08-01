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

static struct block_device *fs_blockdev;

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

char sdcard_buf[2048] = { 0 };
static struct sdhc sdhc;

void hexdump(const uint8_t *buffer, uint32_t size) {
    for (uint32_t i = 0; i < size; i += 16) {
        printf("%08x: ", i);
        for (uint32_t j = 0; j < 16 && i + j < size; j++)
            printf("%02x ", buffer[i + j]);
        printf("\r\n");
    }
}

static int sdhc_test_io(struct sdhc *s)
{
  int err;

  struct {
    sdhc_io_mode_t mode;
    const char *name;
  } test_modes[] = {
    { SDHC_IO_MODE_BLOCKING_PIO, "BLOCKING_PIO" },
    { SDHC_IO_MODE_BLOCKING_DMA, "BLOCKING_DMA" }
    /* SDHC_IO_MODE_IT_DMA, */
  };

  for (int i = 0; i < ARRAY_SIZE(test_modes); ++i) {
    err = sdhc_set_io_mode(s, test_modes[i].mode);
    if (err != SUCCESS) {
      printf("Failed to set sdhc io mode %s\r\n", test_modes[i].name);
      return err;
    }

    printf("Testing SDHC with mode %s\r\n", test_modes[i].name);
    err = sdhc_read(s, sdcard_buf, 0, 4);
    if (err) {
      printf("failed to read from SD\r\n");
      return err;
    }

    hexdump(sdcard_buf, 512);
  }
#if 0
  for (int i = 0; i < 512 * 4 / 4; ++i) {
    sdcard_buf[i * 4 + 0] = i & 0xff;
    sdcard_buf[i * 4 + 1] = i / 4;
    sdcard_buf[i * 4 + 2] = 0x11;
    sdcard_buf[i * 4 + 3] = 0xee;
  }

  err = sdhc_write(&sdhc, sdcard_buf, 1056768, 4);
  if (err) {
    printf("failed to write to SD\r\n");
    while(1)
      asm volatile("wfe");
  }
#endif
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

  err = sdhc_init(&sdhc, &bcm_sdhost_ops);
  if (err != SUCCESS) {
    printf("Failed to init sdhc, %d\r\n", err);
    goto out;
  }

  err = sdhc_test_io(&sdhc);
  if (err != SUCCESS)
    goto out;

  blockdev_scheduler_init();
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

static void vchiq_main(void)
{
  int err;
  struct block_device *bd;

  err = ili9341_init();
  if (err != SUCCESS) {
    printf("Failed to init display, %d\r\n", err);
    while(1)
      asm volatile("wfe");
  }
  printf("----\r\n");

  os_log("vchiq_start\r\n");

  os_wait_ms(100);
  err = fs_init(&bd);
  if (err != SUCCESS) {
    os_log("Failed to init fs block device, err: %d\r\n", err);
    goto out;
  }

  readbuf = dma_alloc(32768, 0);
  memset(readbuf, 0x11, 32768);
  err = bd->ops.read(bd, readbuf, 0, 1);
  printf("readbuf: %d, %08x, %08x\r\n", err, *(uint32_t *)readbuf, *(uint32_t *)(readbuf+4));

  err = ili9341_init();
  if (err != SUCCESS) {
    printf("Failed to init ili9341 display, err: %d\r\n", err);
  }
  vchiq_init(bd);
out:
  os_exit_current_task();
}

#if 0
static void test_dma(void)
{
  uint64_t pa;
  bool success;
  struct bcm2835_dma_request_param p = { 0 };

  void *src = dma_alloc(4096);
  void *dst = dma_alloc(4096);
  if (!src || !dst)
  {
    printf("Failed to test dma\n");
    return;
  }
  memset(src, 0x7, 256);
  success = mmu_get_pddr((uint64_t)src, &pa);
  printf("%p->%lx\n", src, pa);
  p.dreq = 0;
  p.dreq_type = BCM2835_DMA_DREQ_TYPE_NONE;
  p.src = 0xc0000000 | (uint32_t)(uint64_t)src;
  p.dst = 0xc0000000 | (uint32_t)(uint64_t)dst;
  p.src_type = BCM2835_DMA_ENDPOINT_TYPE_INCREMENT;
  p.dst_type = BCM2835_DMA_ENDPOINT_TYPE_INCREMENT;
  p.len = 16;

  bcm2835_dma_requests(&p, 1);
}
#endif

static void kernel_run(void)
{
  struct task *t;
#if 0
  mmu_print_va(0x0000000001ff1000, 1);
  mmu_print_va(0xffff000001ff0000, 1);
  mmu_print_va(0xffff000001ff1000, 1);
  mmu_print_va(0xffff00000201c000, 1);
#endif
  // test_dma();

  t = task_create(vchiq_main, "vchiq_main");
  sched_run_task_isr(t);
  t = task_create(blockdev_scheduler_fn, "block-sched");
  sched_run_task_isr(t);
  bcm2835_emmc_set_interrupt_mode();
  scheduler_start();
  panic();
}

void SECTION(".text") main(void)
{
  kernel_init();
  kernel_run();
}

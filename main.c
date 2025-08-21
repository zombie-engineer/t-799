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
#include "sdhc_test.h"
#include <fs/fs.h>
#include <fs/fat32.h>
#include <logger.h>
#include <list_fifo.h>

static struct block_device *fs_blockdev;
static struct sdhc sdhc;
struct block_device bdev_sdcard;
struct block_device *bdev_partition;

#if 0
volatile char buf1[1024];
volatile char buf2[1024];
#endif

// volatile int myvar = 10;

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

// atomic_t test_atomic;

// struct event test_ev;

// char *readbuf;

// extern uint64_t sdhost_ts_cmd_start;

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

  err = sdhc_run_self_test(&sdhc, &bdev_sdcard);
  if (err != SUCCESS)
    goto out;

  err = sdhc_set_io_mode(&sdhc, SDHC_IO_MODE_IT_DMA, false);
  if (err != SUCCESS) {
    os_log("Failed to set sdhc io mode IT_DMA\r\n");
    goto out;
  }

  err = fs_init(&bdev_sdcard, &bdev_partition);
  if (err != SUCCESS) {
    os_log("Failed to init fs block device, err: %d\r\n", err);
    goto out;
  }

  sdhc_perf_measure(bdev_partition);

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

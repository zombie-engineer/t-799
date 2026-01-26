#include <kmalloc.h>
#include <drivers/timer/timer_bcm2835.h>
#include <drivers/pll/pll_bcm2835.h>
#include <drivers/sd/sdhost_bcm2835.h>
#include <drivers/dma/dma_bcm2835.h>
#include <debug_led.h>
#include <drivers/mbox/mbox_bcm2835.h>
#include <uart_pl011.h>
#include <drivers/mbox/mbox_bcm2835_props.h>
#include <irq.h>
#include <sched.h>
#include <task.h>
#include <os_api.h>
#include <printf.h>
#include <sections.h>
#include <errcode.h>
#include <logger.h>
#include <app/app_main.h>
#include <config.h>

EXCEPTION void fiq_handler(void)
{
}

EXCEPTION void serror_handler(void)
{
}

static void print_mbox_props(void)
{
  uint64_t val64;
  int val, val2, i;
  unsigned clock_rate;
  char buf[7];
  mbox_get_firmware_rev((uint32_t *)&val);
  printf("firmware rev:    %08x\r\n", val);
  mbox_get_board_model((uint32_t *)&val);
  printf("board model:     %08x\r\n", val);
  mbox_get_board_rev((uint32_t *)&val);
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

  uart_pl011_init(CONFIG_CONSOLE_BAUDRATE);
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

static void kernel_run(void)
{
  struct task *t;
  t = task_create(app_main, "app_main");
  sched_run_task_isr(t);
  t = task_create(blockdev_scheduler_fn, "block-sched");
  sched_run_task_isr(t);
  scheduler_start();

  /* not reachable code */
  panic();
}

void SECTION(".text") main(void)
{
  kernel_init();
  kernel_run();
}

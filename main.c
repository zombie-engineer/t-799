#include <gpio.h>
#include <cpu.h>
#include <bcm2835/bcm2835_systimer.h>
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

volatile char buf1[1024];
volatile char buf2[1024];

volatile int myvar = 10;

void fiq_handler(void)
{
}

void serror_handler(void)
{
}

void print_mbox_props(void)
{
	uint64_t val64;
	uint32_t val, val2, i;
	unsigned clock_rate;
	char buf[6];
	mbox_get_firmware_rev(&val);
	printf("firmware rev:		%08x\n", val);
	mbox_get_board_model(&val);
	printf("board model:		 %08x\n", val);
	mbox_get_board_rev(&val);
	printf("board rev:			 %08x\n", val);
	mbox_get_board_serial(&val64);
	printf("board serial:		%08x\n", val64);
	mbox_get_mac_addr(&buf[0], &buf[7]);
	printf("mac address:		 %02x:%02x:%02x:%02x:%02x:%02x\n",
		(int)(buf[0]),
		(int)(buf[1]),
		(int)(buf[2]),
		(int)(buf[3]),
		(int)(buf[4]),
		(int)(buf[5]),
		(int)(buf[6]));

	mbox_get_arm_memory(&val, &val2);
	printf("arm memory base: %08x\n", val);
	printf("arm memory size: %08x\n", val2);

	mbox_get_vc_memory(&val, &val2);
	printf("vc memory base:	%08x\n", val);
	printf("vc memory size:	%08x\n", val2);
	for (i = 0; i < 8; ++i) {
		mbox_get_clock_rate(i, &clock_rate);
		// 	printf("failed to get clock rate for clock_id %d\n", i);
		printf("clock %d rate: %08x (%d KHz)\n", i, clock_rate, clock_rate / 1000);
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
	uart_pl011_init(115200);
	clear_reboot_request();
	print_mbox_props();
	irq_init();
	bcm2835_systimer_init();
	irq_disable();
	mem_allocator_init();
	scheduler_init();
	debug_led_init();
}

atomic_t test_atomic;

struct event test_ev;

static void kernel_start_task1(void)
{
	while(1) {
		os_wait_ms(1000);
		os_yield();
		atomic_cmp_and_swap(&test_atomic, 0, 1);
		printf("waiting \n");
		os_event_clear(&test_ev);
		os_event_wait(&test_ev);
		printf("wait complete\n");
	}
}

static void kernel_start_task2(void)
{
	while(1) {
		os_wait_ms(5000);
		atomic_cmp_and_swap(&test_atomic, 1, 0);
		os_yield();
		os_event_notify(&test_ev);
	}
}

static void kernel_run(void)
{
	struct task *t;

	printf("Hello %d\n", myvar);
	t = task_create(kernel_start_task1, "t1");
	scheduler_start_task(t);
	t = task_create(kernel_start_task2, "t2");
	scheduler_start_task(t);
	scheduler_start();
	panic();
}

void main(void)
{
	kernel_init();
	kernel_run();
}

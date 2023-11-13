#include <gpio.h>
#include <cpu.h>
#include <bcm2835/bcm2835_systimer.h>
#include <common.h>
#include "uart_pl011.h"
#include <stringlib.h>
#include <mbox.h>
#include <mbox_props.h>
#include <irq.h>

volatile char buf1[1024];
volatile char buf2[1024];

#define PRINTF_BUF_SIZE 1024

char printfbuf[PRINTF_BUF_SIZE];

const char * _printf(const char* fmt, ...)
{
	const char *res = fmt;
	__builtin_va_list args;
	__builtin_va_start(args, fmt);
	vsnprintf(printfbuf, sizeof(printfbuf), fmt, &args);
	uart_pl011_send(printfbuf, 0);
	return res;
}
#define printf _printf

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


#define GPIO_IDX_NATIVE_DEBUG_LED 29

static void debug_led_init(void)
{
	gpio_set_pin_function(GPIO_IDX_NATIVE_DEBUG_LED, GPIO_FUNCTION_OUTPUT);
	gpio_set_pin_output(GPIO_IDX_NATIVE_DEBUG_LED, 1);
}

static void debug_led_toggle(void)
{
	gpio_toggle_pin_output(GPIO_IDX_NATIVE_DEBUG_LED);
}


volatile int global_timer = 0;
volatile int global_timer2 = 0;

static void timer_callback_irq(void *arg)
{
	bcm2835_systimer_clear_irq(1);
	global_timer++;
	debug_led_toggle();
	bcm2835_systimer_start_oneshot(1000000, timer_callback_irq, NULL);
}

void main(void)
{
	uart_pl011_init(115200);
	clear_reboot_request();
	print_mbox_props();
	irq_init();
	bcm2835_systimer_init();
	debug_led_init();
	global_timer = 0;
	global_timer2 = 0;
	irq_disable();
	bcm2835_systimer_start_oneshot(600000, timer_callback_irq, NULL);

	while(1) {
		global_timer2++;
		if (global_timer2 == 1000)
			irq_enable();
	}
	asm volatile("svc 1");
	sprintf((char *)buf1, "test_sprintf '44'->%d", 44);
	uart_pl011_send((const void *)buf1, 0);
	panic();
}

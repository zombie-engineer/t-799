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

void irq_handler(void)
{
	gpio_set_pin_function(29, GPIO_FUNCTION_OUTPUT);
	gpio_set_pin_output(29, 1);
}

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
	global_timer++;
	bcm2835_systimer_clear_irq(1);
	bcm2835_systimer_start_oneshot(60000, timer_callback_irq, NULL);
}

static void test_context_switch(void)
{
	volatile register long x0 asm ("x0") = 0;
	volatile register long x1 asm ("x1") = 0;
	volatile register long x2 asm ("x2") = 0;
	volatile register long x3 asm ("x3") = 0;
	volatile register long x4 asm ("x4") = 0;
	volatile register long x5 asm ("x5") = 0;
	volatile register long x6 asm ("x6") = 0;
	volatile register long x7 asm ("x7") = 0;
	volatile register long x8 asm ("x8") = 0;
	volatile register long x9 asm ("x9") = 0;
	volatile register long x10 asm ("x10") = 0;
	volatile register long x11 asm ("x11") = 0;
	volatile register long x12 asm ("x12") = 0;
	volatile register long x13 asm ("x13") = 0;
	volatile register long x14 asm ("x14") = 0;
	volatile register long x15 asm ("x15") = 0;
	volatile register long x16 asm ("x16") = 0;
	volatile register long x17 asm ("x17") = 0;
	volatile register long x18 asm ("x18") = 0;
	volatile register long x19 asm ("x19") = 0;
	volatile register long x20 asm ("x20") = 0;
	volatile register long x21 asm ("x21") = 0;
	volatile register long x22 asm ("x22") = 0;
	volatile register long x23 asm ("x23") = 0;
	volatile register long x24 asm ("x24") = 0;
	volatile register long x25 asm ("x25") = 0;
	volatile register long x26 asm ("x26") = 0;
	volatile register long x27 asm ("x27") = 0;
	volatile register long x28 asm ("x28") = 0;
	volatile register long x29 asm ("x29") = 0;
	int i;
	while(1) {
		i++;
		x0++;
		x1++;
		x2++;
		x3++;
		x4++;
		x5++;
		x6++;
		x7++;
		x8++;
		x9++;
		x10++;
		x11++;
		x12++;
		x13++;
		x14++;
		x15++;
		x16++;
		x17++;
		x18++;
		x19++;
		x20++;
		x21++;
		x22++;
		x23++;
		x24++;
		x25++;
		x26++;
		x27++;
		x28++;
		x29++;
		asm volatile ("svc 0");
	}
}

void main(void)
{

	uart_pl011_init(115200);
	clear_reboot_request();
	print_mbox_props();
	irq_init();
	bcm2835_systimer_init();
	irq_disable();
	bcm2835_systimer_start_oneshot(60000, timer_callback_irq, NULL);

	test_context_switch();
	while(1) {
		global_timer2++;
	}
	asm volatile("svc 1");
	sprintf((char *)buf1, "test_sprintf '44'->%d", 44);
	uart_pl011_send((const void *)buf1, 0);
	panic();
}

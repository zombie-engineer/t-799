#include "uart_pl011.h"
#include "gpio.h"
// #define uart_init uart_pl011_init
#define uart_send uart_pl011_send
#include "bcm2835_gpio.h"
#include <mbox_props.h>

static inline void test_gpio(void)
{
	/*
	 * To test that the binary is starting at proper location on early stage
	 * this code can be put to _start as a first instruction to run:
	 *   mov x0, #0x3f200000
	 *   mov w1, #0x40
	 *   str w1, [x0]
	 *   add x0, x0, #0x1c
	 *   mov w1, #(1<<2)
	 *   str w1, [x0]
	 * 1:
	 *   wfe
	 *   b 1b
	 *
	 * Same thing done with gpio API is run below:
	*/

	gpio_set_pin_function(2, GPIO_FUNCTION_OUTPUT);
	gpio_set_pin_output(2, true);
	while(1) {
		asm volatile ("wfe");
	}
}

#ifdef CONFIG_JTAG_ENABLE_AT_RUNTIME
static inline void jtag_enable(void)
{
	volatile int wait_on_jtag = 1;
	gpio_set_pin_function(GPIO_PIN_ARM_TRST_B, GPIO_FUNCTION_ALT_4);
	gpio_set_pin_function(GPIO_PIN_ARM_RTCK_B, GPIO_FUNCTION_ALT_4);
	gpio_set_pin_function(GPIO_PIN_ARM_TDO_B, GPIO_FUNCTION_ALT_4);
	gpio_set_pin_function(GPIO_PIN_ARM_TCK_B, GPIO_FUNCTION_ALT_4);
	gpio_set_pin_function(GPIO_PIN_ARM_TDI_B, GPIO_FUNCTION_ALT_4);
	gpio_set_pin_function(GPIO_PIN_ARM_TMS_B, GPIO_FUNCTION_ALT_4);

	while(wait_on_jtag)
		asm volatile ("wfe");
}
#endif

void main(void)
{
	char mac[6];
	uint64_t serial;
#ifdef CONFIG_JTAG_ENABLE_AT_RUNTIME
	jtag_enable();
#endif
	mbox_get_board_serial(&serial);
	mbox_get_mac_addr(mac, mac + 6);
}

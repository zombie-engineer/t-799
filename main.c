#include "uart_pl011.h"
#include "gpio.h"

// #define uart_init uart_pl011_init
#define uart_send uart_pl011_send

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

void main(void)
{
	test_gpio();
	uart_pl011_init();
	uart_send("hello\n", 6);
}

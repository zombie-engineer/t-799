#include <gpio.h>
// #include "bcm2835_gpio.h"
#include <common.h>
#include "uart_pl011.h"
#include <stringlib.h>

volatile char buf1[1024];
volatile char buf2[1024];

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

void main(void)
{
	uart_pl011_init(115200);
	clear_reboot_request();
	asm volatile("svc 1");
	sprintf((char *)buf1, "test_sprintf '44'->%d", 44);
	uart_pl011_send((const void *)buf1, 0);
	panic();
}

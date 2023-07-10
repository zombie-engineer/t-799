#include "gpio.h"
#include "bcm2835_gpio.h"
#include <common.h>
#include <stringlib.h>

volatile char buf1[1024];
volatile char buf2[1024];
void main(void)
{
	clear_reboot_request();
	gpio_set_pin_function(29, GPIO_FUNCTION_OUTPUT);
	gpio_set_pin_output(29, 1);
	memcpy((void *)buf1, (void *)buf2, 1024);

	panic();
}

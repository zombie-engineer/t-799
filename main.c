#include "gpio.h"
#include "bcm2835_gpio.h"
#include <common.h>

void main(void)
{
	clear_reboot_request();
	gpio_set_pin_function(29, GPIO_FUNCTION_OUTPUT);
	gpio_set_pin_output(29, 1);

	panic();
}

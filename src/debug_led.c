#include <debug_led.h>
#include <drivers/gpio.h>

#define GPIO_IDX_NATIVE_DEBUG_LED 29

void debug_led_init(void)
{
  gpio_set_pin_function(GPIO_IDX_NATIVE_DEBUG_LED, GPIO_FUNCTION_OUTPUT);
  gpio_set_pin_output(GPIO_IDX_NATIVE_DEBUG_LED, 1);
}

void debug_led_toggle(void)
{
  gpio_toggle_pin_output(GPIO_IDX_NATIVE_DEBUG_LED);
}

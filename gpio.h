#pragma once
#include <stdbool.h>

/*
 * gpio_set_pin_function - selects one of the functions
 * for selected pin
 */
typedef enum {
	GPIO_FUNCTION_INPUT,
	GPIO_FUNCTION_OUTPUT,
	GPIO_FUNCTION_ALT_0,
	GPIO_FUNCTION_ALT_1,
	GPIO_FUNCTION_ALT_2,
	GPIO_FUNCTION_ALT_3,
	GPIO_FUNCTION_ALT_4,
	GPIO_FUNCTION_ALT_5
} gpio_function_t;

void gpio_set_pin_function(int pin, gpio_function_t function);

/*
 * gpio_set_pin_pud_mode - selects pull up down mode for
 * selected pin
 * pin - index of gpio pin
 * mode - one of modes (pull-up, pull-down, or tristate)
 */
typedef enum {
	GPIO_PULLUPDOWN_MODE_TRISTATE,
	GPIO_PULLUPDOWN_MODE_DOWN,
	GPIO_PULLUPDOWN_MODE_UP,
} gpio_pullupdown_mode_t;
void gpio_set_pin_pullupdown_mode(int pin, gpio_pullupdown_mode_t mode);

/*
 * gpio_set_pin_output - sets logical high or low on a
 * selected pin.
 * is_set - if value is true, pin is set to logical high
 *          if false - to logical low
 */
void gpio_set_pin_output(int pin, bool is_set);


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

typedef enum {
	GPIO_PIN_0 = 0,
	GPIO_PIN_1,
	GPIO_PIN_2,
	GPIO_PIN_3,
	GPIO_PIN_4,
	GPIO_PIN_5,
	GPIO_PIN_6,
	GPIO_PIN_7,
	GPIO_PIN_8,
	GPIO_PIN_9,
	GPIO_PIN_10,
	GPIO_PIN_11,
	GPIO_PIN_12,
	GPIO_PIN_13,
	GPIO_PIN_14,
	GPIO_PIN_15,
	GPIO_PIN_16,
	GPIO_PIN_17,
	GPIO_PIN_18,
	GPIO_PIN_19,
	GPIO_PIN_20,
	GPIO_PIN_21,
	GPIO_PIN_22,
	GPIO_PIN_23,
	GPIO_PIN_24,
	GPIO_PIN_25,
	GPIO_PIN_26,
	GPIO_PIN_27,
	GPIO_PIN_28,
	GPIO_PIN_29,
	GPIO_PIN_30,
	GPIO_PIN_31
} gpio_pin_t;

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


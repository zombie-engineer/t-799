#include "uart_pl011.h"
#include "gpio.h"
#include "bcm2835_gpio.h"
#include <mbox_props.h>
#include <spi.h>
#include <errcode.h>
#include <io_flags.h>
#include "ioreg.h"
#include <stddef.h>

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

static inline void reboot(void)
{
  ioreg32_write((ioreg32_t)0x3f100024, (0x5a << 24) | 1);
  ioreg32_write((ioreg32_t)0x3f10001c, (0x5a << 24) | 0x20);
  while(1) asm volatile("wfe");
}

volatile bool should_reboot = 0;

void panic(void)
{
	while(!should_reboot)
		asm volatile ("wfe");
	reboot();
}

void test_spi(void)
{
	int len;
	int actual_len;
	struct spi_device *spi;
	const uint8_t payload[] = { 0b10101010, 0b11110000 };
	spi = spi_get_device(0);
	if (!spi)
		panic();

	if (!spi->fns || !spi->fns->do_transfer)
		panic();

	if (spi->fns->init && spi->fns->init(spi) != SUCCESS)
		panic();

	/* testing SPI write */
	while(1) {
		for (volatile int x = 0; x < 40000; ++x);
		if (spi->fns->start_transfer &&
		spi->fns->start_transfer(spi, IO_FLAG_WRITE, 0) != SUCCESS)
		panic();

		for (int i = 0; i < 100; ++i) {
			if (spi->fns->do_transfer(spi, payload, NULL, sizeof(payload),
				&actual_len) != SUCCESS) {
				panic();
			}
		}

		if (spi->fns->finish_transfer &&
			spi->fns->finish_transfer(spi) != SUCCESS)
			panic();
	}
}

void main(void)
{
	should_reboot = false;

	test_spi();

	panic();
	char mac[6];
	uint64_t serial;
#ifdef CONFIG_JTAG_ENABLE_AT_RUNTIME
	jtag_enable();
#endif
	mbox_get_board_serial(&serial);
	mbox_get_mac_addr(mac, mac + 6);
	uart_pl011_init(115200);
	while(1)
		uart_pl011_send("hello\n", 6);
}

#include "uart_pl011.h"

// #define uart_init uart_pl011_init
#define uart_send uart_pl011_send

void kernel_c_entry(void)
{
	uart_pl011_init();
	uart_send("hello\n", 6);
}

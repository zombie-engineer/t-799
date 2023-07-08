#include "common.h"
#include <stdbool.h>
#include "ioreg.h"

static volatile bool should_reboot = false;

void request_reboot(void)
{
	should_reboot = true;
}

void clear_reboot_request(void)
{
	should_reboot = false;
}

void reboot(void)
{
	ioreg32_write((ioreg32_t)0x3f100024, (0x5a << 24) | 1);
	ioreg32_write((ioreg32_t)0x3f10001c, (0x5a << 24) | 0x20);
	while(1)
		asm volatile("wfe");
}

void panic(void)
{
	while(!should_reboot)
		asm volatile ("wfe");
	reboot();
}


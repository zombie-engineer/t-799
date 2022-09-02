#include "uart_pl011.h"
#include "types.h"
#include <assert.h>

#define BASE_ADDR 0x3f000000
#define PL011_BASE (BASE_ADDR + 0x201000)

typedef struct {
	uint32_t dr;
	uint32_t rsrecr;
	char __padding_0[0x10];
	uint32_t fr;
	char __padding_1[4];
	uint32_t ilpr;
	uint32_t ibrd;
	uint32_t fbrd;
	uint32_t lcrh;
	uint32_t cr;
	uint32_t ifls;
	uint32_t imsc;
	uint32_t ris;
	uint32_t mis;
	uint32_t icr;
	uint32_t dmacr;
	char __padding[0x34];
	uint32_t itcr;
	uint32_t itip;
	uint32_t itop;
	uint32_t tdr;
} __attribute__((packed)) regs_pl011;

#define ASSERT_FIELD_OFFSET(__struct, __field, __offset) \
	static_assert(&((__struct *)0)->__field == (void *)__offset, \
			"In struct " #__struct ", field " #__field " not at expected offset " #__offset  );

ASSERT_FIELD_OFFSET(regs_pl011, fr, 0x18);
ASSERT_FIELD_OFFSET(regs_pl011, ilpr, 0x20);
ASSERT_FIELD_OFFSET(regs_pl011, dmacr, 0x48);
ASSERT_FIELD_OFFSET(regs_pl011, itcr, 0x80);

#define pl011_uart ((regs_pl011 *)PL011_BASE)

int uart_pl011_init(void)
{
	pl011_uart->ibrd = 3000000 / (16 * 115200);
	pl011_uart->fbrd = 3000000 / (16 * 115200);
	pl011_uart->lcrh = 3 << 5;
	pl011_uart->cr = 0x301;
	return 0;
}

int uart_pl011_send(const void *buf, int num)
{
	const char *ptr = buf;
	while(*ptr && --num)
	{
		pl011_uart->dr = *ptr++;
	}
	return ptr - (const char *)buf;
}

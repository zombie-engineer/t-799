#include "uart_pl011.h"
#include "bcm2835_gpio.h"
#include <mbox_props.h>
#include <gpio.h>
#include <stdint.h>
#include <assert.h>

#define BASE_ADDR 0x3f000000
#define PL011_BASE (BASE_ADDR + 0x201000)

#define PL011_LCRH_BRK_POS 0
#define PL011_LCRH_PEN_POS 1
#define PL011_LCRH_PEN_PARITY_OFF 0
#define PL011_LCRH_PEN_PARITY_ON 1
#define PL011_LCRH_EPS_POS 2
#define PL011_LCRH_EPS_PARITY_ODD 0
#define PL011_LCRH_EPS_PARITY_EVEN 1
#define PL011_LCRH_STP2_POS 3
#define PL011_LCRH_FEN_POS 4
#define PL011_LCRH_WLEN_POS 5
#define PL011_LCRH_WLEN_8BITS 3
#define PL011_LCRH_WLEN_7BITS 2
#define PL011_LCRH_WLEN_6BITS 1
#define PL011_LCRH_WLEN_5BITS 0

#define PL011_CR_UARTEN_POS 0
#define PL011_CR_TXE_POS 8
#define PL011_CR_RXE_POS 9
#define PL011_CR_RTS_POS 11
#define PL011_CR_RTSEN_POS 14
#define PL011_CR_CTSEN_POS 15


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

static void pl011_calc_divisor(int baudrate, uint64_t clock_hz, uint32_t *idiv, uint32_t *fdiv)
{
  /* Calculate integral and fractional parts of divisor, using the formula
   * from the bcm2835 manual.
   * DIVISOR = CLOCK_HZ / (16 * BAUDRATE)
   * where DIVISOR is represented by idiv.fdiv floating point number,
   * where idiv is intergral part of DIVISOR and fdiv is fractional
   * part of DIVISOR
   * ex: 2.11 = 4 000 000 Hz / (16 * 115200), idiv = 2, fdiv = 11
   */

  uint64_t div;
  uint32_t i, f;
  /* Defaults for a 4Mhz clock */
  // *idiv = 2;
  // *fdiv = 11;

  /* Defaults for a 48Mhz clock */
  // *idiv = 26;
  // *fdiv = 0;

  div = clock_hz * 10 / (16 * baudrate);
  i = div / 10;
  f = div % 10;
  if (i > 0xffff)
    return;

  if (f > 0b11111)
    f = 0b11111;

  *idiv = i;
  *fdiv = f;
}

bool uart_pl011_init(int baudrate)
{
  uint32_t idiv, fdiv, clock_rate_hz;

  gpio_set_pin_function(GPIO_PIN_UART_TXD0, GPIO_FUNCTION_ALT_0);
  gpio_set_pin_function(GPIO_PIN_UART_RXD0, GPIO_FUNCTION_ALT_0);

  gpio_set_pin_pullupdown_mode(GPIO_PIN_UART_RXD0,
    GPIO_PULLUPDOWN_MODE_DOWN);

  if (!mbox_get_clock_rate(2, &clock_rate_hz)) {
    return false;
  }

  pl011_calc_divisor(baudrate, clock_rate_hz, &idiv, &fdiv);

  pl011_uart->ibrd = idiv;
  pl011_uart->fbrd = fdiv;
  pl011_uart->lcrh = (PL011_LCRH_WLEN_8BITS << PL011_LCRH_WLEN_POS);
  pl011_uart->icr = 0x7ff;

  pl011_uart->cr = (1<<PL011_CR_UARTEN_POS)
    | (1<<PL011_CR_TXE_POS)
    | (1<<PL011_CR_RXE_POS);

  return true;
}

int uart_pl011_send(const void *buf, int num)
{
  const char *ptr = buf;
  while(*ptr) {
    while(pl011_uart->fr & (1<<3));
    pl011_uart->dr = *ptr++;
  }
  return ptr - (const char *)buf;
}

int uart_pl011_recv(void *buf, int num)
{
  int i;
  char *ptr = buf;

  for (i = 0; i < num; ++i) {
    while(pl011_uart->fr & (1<<4));
    *ptr = pl011_uart->dr;
  }
  return num;
}

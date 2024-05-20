#pragma once
#include <uart_pl011.h>
#include <stringlib.h>
#include <cpu.h>

extern char printfbuf[];
extern size_t printfbuf_size;

static inline const char * _printf(const char* fmt, ...)
{
  int irqflags;

  const char *res = fmt;

  disable_irq_save_flags(irqflags);
  __builtin_va_list args;
  __builtin_va_start(args, fmt);
  vsnprintf(printfbuf, printfbuf_size, fmt, &args);
  uart_pl011_send(printfbuf, 0);
  restore_irq_flags(irqflags);
  return res;
}

static inline void puts(const char *string)
{
  int irqflags;
  disable_irq_save_flags(irqflags);
  uart_pl011_send(string, 0);
  restore_irq_flags(irqflags);
}

static inline void putc(char c)
{
  int irqflags;
  disable_irq_save_flags(irqflags);
  uart_pl011_send_char(c);
  restore_irq_flags(irqflags);
}
#define printf _printf

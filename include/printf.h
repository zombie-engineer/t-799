#pragma once
#include <uart_pl011.h>
#include <stringlib.h>

extern char printfbuf[];
extern size_t printfbuf_size;

static inline const char * _printf(const char* fmt, ...)
{
  const char *res = fmt;
  __builtin_va_list args;
  __builtin_va_start(args, fmt);
  vsnprintf(printfbuf, printfbuf_size, fmt, &args);
  uart_pl011_send(printfbuf, 0);
  return res;
}

#define printf _printf

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <compiler.h>

#define LOGGER_MAX_ENTRY_LENGTH 256

#define LOGGER_ENT_STATE_FREE 0
#define LOGGER_ENT_STATE_BUSY 1
#define LOGGER_ENT_STATE_READY 2
struct logger_entry {
  uint8_t state;
  /* Place to store log text */
  char buf[LOGGER_MAX_ENTRY_LENGTH];
} PACKED;

int logger_init(void);

void __os_log(const char *fmt, __builtin_va_list *args);

static inline void os_log(const char *fmt, ...)
{
  __builtin_va_list args;
  __builtin_va_start(args, fmt);
  __os_log(fmt, &args);
}

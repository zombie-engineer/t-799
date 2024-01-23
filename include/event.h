#pragma once
#include <stdint.h>

struct event {
  uint64_t ev;
};

#define EVENT_INIT { .ev = 0 }


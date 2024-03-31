#pragma once
#include <string.h>
#define MIN(__a, __b) (__a) < (__b) ? (__a) : (__b)
#define ARRAY_SIZE(__a) (sizeof(__a)/sizeof(__a[0]))
#define BUG_IF(__cond, __msg) if (__cond) { printf(__msg); }

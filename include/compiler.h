#pragma once

#define NOINLINE __attribute__((noinline))
#define OPTIMIZED __attribute__((optimize("O2")))
#define ALIGNED(bytes) __attribute__((aligned(bytes)))
#define PACKED __attribute__((packed))
#define NORETURN __attribute__((noreturn))
#define SECTION(name)  __attribute__((section(name)))

#define STRICT_SIZE(__type, __size) \
  _Static_assert(sizeof(__type) == __size, \
    "compile-time size mismatch: sizeof("#__type") != "#__size);

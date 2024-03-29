#pragma once

#define NOINLINE __attribute__((noinline))
#define OPTIMIZED __attribute__((optimize("O2")))
#define ALIGNED(bytes) __attribute__((aligned(bytes)))
#define PACKED __attribute__((packed))
#define NORETURN __attribute__((noreturn))
#define SECTION(name)  __attribute__((section(name)))

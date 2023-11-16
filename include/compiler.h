#pragma once

#define NOINLINE __attribute__((noinline))
#define OPTIMIZED __attribute__((optimize("O2")))
#define ALIGNED(bytes) __attribute__((aligned(bytes)))
#define PAKCED __attribute__((packed))
#define NORETURN __attribute__((noreturn))

#pragma once

#define NOINLINE __attribute__((noinline))
#define OPTIMIZED __attribute__((optimize("O2")))
#define ALIGNED(bytes) __attribute__((aligned(bytes)))

#pragma once
#include <compiler.h>
#include <stdint.h>

struct armv8_cpuctx {
  uint64_t gpregs[31];
  uint64_t sp;
  uint64_t pc;
  uint64_t pstate;
} PACKED;

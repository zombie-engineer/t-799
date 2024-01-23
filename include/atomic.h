#pragma once
#include <stdint.h>

typedef uint64_t atomic_t;

atomic_t atomic_cmp_and_swap(atomic_t *a, uint64_t expected_val,
  uint64_t new_val);

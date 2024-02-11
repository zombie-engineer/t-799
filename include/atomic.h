#pragma once
#include <stdint.h>

typedef uint64_t atomic_t;

atomic_t atomic_cmp_and_swap(atomic_t *a, uint64_t expected_val,
  uint64_t new_val);

atomic_t atomic_inc(atomic_t *a);

atomic_t atomic_dec(atomic_t *a);

static inline void atomic_set(atomic_t *a, uint64_t val)
{
  *a = val;
}

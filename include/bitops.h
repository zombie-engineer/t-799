#pragma once

#define __CAST_TO_64BITS(__v) ((uint64_t)(__v))


#define BIT(__n) (1 << (__n))
#define BITS(__n, __bits) ((__bits) << (__n))
#define BIT_ENABLED(__value, __n) ((__value) & BIT(__n))

#define BITS_EXTRACT64(__v, __pos, __mask) \
  ((__CAST_TO_64BITS(__v) & (__mask)) << (__pos))

#define BIT_EXTRACT64(__v, __pos) BITS_EXTRACT64(__v, __pos, 1)

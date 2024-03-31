#pragma once
#include <stdint.h>

#define TO_64(__v) ((uint64_t)(__v))
#define TO_32(__v) ((uint32_t)(__v))


#define BIT(__n) (1 << (__n))
#define BITS(__n, __bits) ((__bits) << (__n))
#define BIT_ENABLED(__value, __n) ((__value) & BIT(__n))
#define WIDTH_TO_MASK64(__width) ((TO_64(1) << (__width)) - 1)
#define WIDTH_TO_MASK32(__width) TO_32(WIDTH_TO_MASK64(__width))

#define BIT_IS_SET(__val, __pos) (__val & BIT(__pos))

#define BIT_IS_CLEAR(__val, __pos) (!BIT_IS_SET(__val, __pos))

#define BITS_EXTRACT32(__v, __pos, __width) \
  ((TO_32(__v) >> (__pos)) & WIDTH_TO_MASK64(__width))

#define BITS_EXTRACT64(__v, __pos, __width) \
  ((TO_64(__v) >> (__pos)) & WIDTH_TO_MASK64(__width))

#define BF_MAKE_MASK32(__pos, __width) \
  (WIDTH_TO_MASK32((__width)) << (__pos))

#define BF_MAKE_MASK64(__pos, __width) (WIDTH_TO_MASK64(__width) << (__pos))

#define BIT_EXTRACT32(__v, __pos) BITS_EXTRACT32(__v, __pos, 1)
#define BIT_EXTRACT64(__v, __pos) BITS_EXTRACT64(__v, __pos, 1)

#define B_CLEAR32(__v, __pos, __width) \
  do { \
    __v &= ~BF_MAKE_MASK32(__pos, __width); \
  } while(0)

#define B_ORR32(__v, __set, __pos)\
  do { \
    __v |= (__set) << __pos; \
  } while(0)

#define B_CLEAR_AND_SET32(__v, __set, __pos, __width) \
  do { \
    B_CLEAR32(__v, __pos, __width);\
    B_ORR32(__v, __set, __pos); \
  } while(0)

#define BYTE_EXTRACT64(__v, __pos) BITS_EXTRACT64(__v, (__pos * 8), 8)
#define BYTE_EXTRACT32(__v, __pos) BITS_EXTRACT32(__v, (__pos * 8), 8)

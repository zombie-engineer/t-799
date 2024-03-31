#pragma once
#include <bitops.h>
#include <stdbool.h>

#define __GET_BYTE(__addr, __i) (((char *)__addr)[__i])
#define __SET_BYTE(__addr, __i, __v) ((char *)__addr)[__i] = __v

static inline uint16_t get_unaligned_16_le(const void *addr)
{
  const uint8_t *p = addr;
  uint16_t b0 = p[0];
  uint16_t b1 = p[1];

  return (b1 << 8) | b0;
}

static inline uint32_t get_unaligned_32_le(const void *addr)
{
  const uint8_t *p = addr;
  uint32_t b0 = p[0];
  uint32_t b1 = p[1];
  uint32_t b2 = p[2];
  uint32_t b3 = p[3];

  return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

static inline void set_unaligned_16_le(void *a, uint16_t v)
{
  __SET_BYTE(a, 0, BYTE_EXTRACT32(v, 0));
  __SET_BYTE(a, 1, BYTE_EXTRACT32(v, 1));
}

static inline void set_unaligned_32_le(void *a, uint32_t v)
{
  __SET_BYTE(a, 0, BYTE_EXTRACT32(v, 0));
  __SET_BYTE(a, 1, BYTE_EXTRACT32(v, 1));
  __SET_BYTE(a, 2, BYTE_EXTRACT32(v, 2));
  __SET_BYTE(a, 3, BYTE_EXTRACT32(v, 3));
}

static inline bool is_aligned(void *a, int alignment)
{
  return ((uint64_t)a) & (alignment - 1) ? 0 : 1;
}

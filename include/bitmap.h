#pragma once
#include <bitword.h>
#include <common.h>
#include <string.h>

struct bitmap {
  uint64_t *data;
  size_t num_entries;
};

#define BITMAP_INIT(__data, __n_entries) \
{ .data = __data, .num_entries = __n_entries }

#define BITMAP_NUM_WORDS(__num_entries) \
  ((__num_entries + BITS_PER_WORD64 - 1) / BITS_PER_WORD64)

static inline size_t bitmap_num_bitwords(struct bitmap *b)
{
  return BITMAP_NUM_WORDS(b->num_entries);
}

static inline void bitmap_clear_all(struct bitmap *b)
{
  BUG_IF(!b->data, "bitmap: trying to clear NULL data");
  memset(b->data, 0, bitmap_num_bitwords(b) * sizeof(b->data[0]));
}

/*
 * clear bit in bitmap
 */
static inline void bitmap_clear_entry(struct bitmap *b, int idx)
{
  BUG_IF(idx >= b->num_entries, "bitmap: out of range");

  b->data[idx >> BITS_PER_WORD64_LOG] &= ~(1ull << (idx % BITS_PER_WORD64));
}

/*
 * bitmap_set_next_free - looks for first bit that is not set, sets it
 * and returns it's index. If no bits were clear, returns end_idx
 */
static inline int bitmap_set_next_free(struct bitmap *b)
{
  uint64_t *bitword;
  int bit;
  size_t i;
  uint64_t mask;

  for (i = 0; i < b->num_entries; ++i) {
    mask = (uint64_t)1 << (i % BITS_PER_WORD64);
    bitword = &b->data[i >> BITS_PER_WORD64_LOG];
    if (!(*bitword & mask)) {
      *bitword |= mask;
      return i;
    }
  }

  return -1;
}

static inline bool bitmap_bit_is_set(struct bitmap *b, int idx)
{
  BUG_IF(idx >= b->num_entries, "bitmap: out of range");
  return b->data[idx >> BITS_PER_WORD64_LOG] >> (idx % BITS_PER_WORD64);
}

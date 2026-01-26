#pragma once
#include <stdbool.h>

struct stat_ringbuf {
  unsigned int wr;
  unsigned int rd;
  unsigned int size;
  uint32_t *data;
};

static inline bool stat_ringbuf_get_space_locked(const struct stat_ringbuf *r)
{
  if (r->wr >= r->rd)
    return r->size - r->wr + r->rd - 1;
  return r->rd - r->wr - 1;
}

static inline bool stat_ringbuf_get_count_locked(const struct stat_ringbuf *r)
{
  if (r->wr >= r->rd)
    return r->wr - r->rd;
  return r->size - r->rd + r->wr;
}

static inline bool status_ringbuf_is_empty_locked(const struct stat_ringbuf *r)
{
  return !stat_ringbuf_get_count_locked(r);
}

static inline bool stat_ringbuf_has_space_locked(const struct stat_ringbuf *r)
{
  return stat_ringbuf_get_space_locked(r);
}

static inline void stat_ringbuf_push_locked(struct stat_ringbuf *r,
  uint32_t value)
{
  unsigned int wr;

  if (!stat_ringbuf_has_space_locked(r))
    return;

  wr = r->wr;
  r->data[wr++] = value;
  if (wr == r->size)
    wr = 0;
  r->wr = wr;
}

#define stat_ringbuf_push_isr stat_ringbuf_push_locked

static inline void stat_ringbuf_push(struct stat_ringbuf *r, uint32_t value)
{
  int irq;
  disable_irq_save_flags(irq);
  stat_ringbuf_push_locked(r, value);
  restore_irq_flags(irq);
}

static inline bool stat_ringbuf_pop_locked(struct stat_ringbuf *r,
  uint32_t *value)
{
  unsigned int rd;

  if (status_ringbuf_is_empty_locked(r))
    return false;

  rd = r->rd;
  *value = r->data[rd++];
  if (rd == r->size)
    rd = 0;
  r->rd = rd;
  return true;
}

static inline bool stat_ringbuf_pop(struct stat_ringbuf *r, uint32_t *value)
{
  bool ret;
  int irq;
  disable_irq_save_flags(irq);
  ret = stat_ringbuf_pop_locked(r, value);
  restore_irq_flags(irq);
  return ret;
}

static inline void stat_ringbuf_init(struct stat_ringbuf *r, uint32_t *data,
  size_t size)
{
  r->wr = r->rd = 0;
  r->data = data;
  r->size = size;
}


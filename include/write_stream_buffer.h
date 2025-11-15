#pragma once
#include <stdint.h>
#include <list.h>

struct write_stream_buf {
  struct list_head list;
  uint32_t paddr;
  uint32_t size;
  uint32_t io_size;
  uint32_t io_offset;
  void *priv;
  int dma_cb;
};


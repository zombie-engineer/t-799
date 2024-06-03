#pragma once
#include <stddef.h>
#include <stdint.h>

int ili9341_init(void);
void ili9341_draw_bitmap(const uint8_t *data, size_t data_sz,
  void (*dma_done_cb_irq)(uint32_t));

int ili9341_nonstop_refresh_init(void (*dma_done_cb_irq)(uint32_t));

struct ili9341_buffer_info {
  uint8_t *buffer;
  size_t buffer_size;
  uint32_t handle;
};

int ili9341_nonstop_refresh_get_buffers(struct ili9341_buffer_info *buffers,
  size_t num_buffers);

size_t ili9341_get_nr_display_buffers(void);

int ili9341_draw_dma_buf(uint32_t buf_handle);

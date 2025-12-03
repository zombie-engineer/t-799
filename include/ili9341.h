#pragma once
#include <stddef.h>
#include <stdint.h>
#include <graphics.h>

int ili9341_init(int gpio_blk, int gpio_dc, int gpio_reset);

struct ili9341_region_cfg {
  uint8_t **dma_buffers;
  int num_dma_buffers;
  struct rectangle frame;
};

int ili9341_regions_init(const struct ili9341_region_cfg *regions,
  int num_regions);

void ili9341_draw_bitmap(const uint8_t *data, size_t data_sz,
  void (*dma_done_cb_irq)(uint32_t));

typedef void (*display_dma_done_cb_isr)(uint32_t);

int ili9341_nonstop_refresh_init(display_dma_done_cb_isr cb,
  uint32_t region_x0, uint32_t region_y0,
  uint32_t region_x1, uint32_t region_y1);

struct ili9341_buffer_info {
  uint8_t *buffer;
  size_t buffer_size;
  uint32_t handle;
};

int ili9341_nonstop_refresh_get_buffers(struct ili9341_buffer_info *buffers,
  size_t num_buffers);

size_t ili9341_get_nr_display_buffers(void);

int ili9341_draw_dma_buf(uint32_t buf_handle);

size_t ili9341_get_frame_byte_size(int frame_width, int frame_height);

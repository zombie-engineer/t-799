#pragma once
#include <stddef.h>
#include <stdint.h>

void ili9341_init(void);
void ili9341_draw_bitmap(const uint8_t *data, size_t data_sz,
  void (*dma_done_cb_irq)(void));

int ili9341_nonstop_refresh_init(void (*dma_done_cb_irq)(void));

int ili9341_nonstop_refresh_get_dma_buffer(uint8_t **dma_buf, size_t *sz);

int ili9341_nonstop_refresh_start(void);

int ili9341_nonstop_refresh_poke(void);

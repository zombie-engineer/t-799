#pragma once
#include <stddef.h>
#include <stdint.h>
#include <graphics.h>
#include <list.h>

struct spi_dma_xfer_cb {
  int spi_start;
  int tx;
  int rx;
};

struct ili9341_per_frame_dma {
  struct list_head draw_tasks;
  struct spi_dma_xfer_cb *cbs;
  int num_transfers;
  /* DMA control blocks */
  uint8_t *buf;
  size_t buf_size;
  uint8_t handle;

  int drawframe_idx;
  int dma_io_idx;
};

typedef void (*ili9341_on_dma_done_irq)(struct ili9341_per_frame_dma *buf);

struct ili9341_drawframe {
  struct rectangle frame;
  struct ili9341_per_frame_dma *bufs;
  size_t num_bufs;

  /*
   * spi header - spi DMA header, is the first DMA task to write to SPI control
   * reg to activate SPI transaction. Holds size of transfer and TA bit.
   * Each SPI DMA data transaction is limited by MAX_BYTES_PER_TRANSFER bytes.
   * Therefore if DMA transaction should be bigger (for bigger frames), SPI
   * should be re-activated "DATA_SIZE / MAX_BYTES_PER_TRANSFER" number of 
   * times
   */
  uint32_t spi_header_last;
  int num_transfers;
  ili9341_on_dma_done_irq on_dma_done_irq;
};

int ili9341_init(int gpio_blk, int gpio_dc, int gpio_reset,
  struct ili9341_drawframe *drawframes, int num_drawframes);

void ili9341_draw_bitmap(const uint8_t *data, size_t data_sz,
  ili9341_on_dma_done_irq on_dma_done_irq);

int ili9341_drawframe_set_irq(struct ili9341_drawframe *r,
  ili9341_on_dma_done_irq on_dma_done_irq);

void ili9341_draw_dma_buf(struct ili9341_per_frame_dma *dma_buf);

size_t ili9341_get_frame_byte_size(int frame_width, int frame_height);

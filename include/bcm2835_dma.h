#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define DMA_DREQ_NONE           0
#define DMA_DREQ_DSI            1
#define DMA_DREQ_PCM_TX         2
#define DMA_DREQ_PCM_RX         3
#define DMA_DREQ_SMI            4
#define DMA_DREQ_PWM            5
#define DMA_DREQ_SPI_TX         6
#define DMA_DREQ_SPI_RX         7
#define DMA_DREQ_BSC_SLAVE_TX   8
#define DMA_DREQ_BSC_SLAVE_RX   9
#define DMA_DREQ_UNUSED        10
#define DMA_DREQ_EMMC          11
#define DMA_DREQ_UART_TX       12
#define DMA_DREQ_SD_HOST       13
#define DMA_DREQ_UART_RX       14
#define DMA_DREQ_DSI_B         15
#define DMA_DREQ_SLIMBUS_MCTX  16
#define DMA_DREQ_HDMI          17
#define DMA_DREQ_SLIMBUS_MCRX  18
#define DMA_DREQ_SLIMBUS_DC0   19
#define DMA_DREQ_SLIMBUS_DC1   20
#define DMA_DREQ_SLIMBUS_DC2   21
#define DMA_DREQ_SLIMBUS_DC3   22
#define DMA_DREQ_SLIMBUS_DC4   23
#define DMA_DREQ_SCALER_FIFO_0 24
#define DMA_DREQ_SCALER_FIFO_1 25
#define DMA_DREQ_SCALER_FIFO_2 26
#define DMA_DREQ_SLIMBUS_DC5   27
#define DMA_DREQ_SLIMBUS_DC6   28
#define DMA_DREQ_SLIMBUS_DC7   29
#define DMA_DREQ_SLIMBUS_DC8   30
#define DMA_DREQ_SLIMBUS_DC9   31

typedef enum {
  BCM2835_DMA_ENDPOINT_TYPE_IGNORE,
  BCM2835_DMA_ENDPOINT_TYPE_INCREMENT,
  BCM2835_DMA_ENDPOINT_TYPE_NOINC
} bcm2835_dma_endpoint_type_t;

typedef enum {
  BCM2835_DMA_DREQ_TYPE_NONE = 0,
  BCM2835_DMA_DREQ_TYPE_SRC,
  BCM2835_DMA_DREQ_TYPE_DST
} bcm2835_dma_dreq_type_t;

struct bcm2835_dma_request_param {
  int dreq;
  bcm2835_dma_dreq_type_t dreq_type;
  uint32_t src;
  uint32_t dst;
  bcm2835_dma_endpoint_type_t src_type;
  bcm2835_dma_endpoint_type_t dst_type;
  size_t len;
  bool enable_irq;
};

void bcm2835_dma_init(void);
void bcm2835_dma_enable(int channel);
void bcm2835_dma_irq_enable(int channel);

void bcm2835_dma_reset(int channel);
int bcm2835_dma_requests(const struct bcm2835_dma_request_param *p, size_t n);

int bcm2835_dma_reserve_cb(void);

bool bcm2835_dma_program_cb(const struct bcm2835_dma_request_param *p,
  int cb_handle);

bool bcm2835_dma_link_cbs(int cb_handle, int cb_handle_next);
void bcm2835_dma_set_cb(int channel, int cb_handle);

void bcm2835_dma_poll(int channel);

void bcm2835_dma_activate(int channel);

bool bcm2835_dma_set_irq_callback(int channel, void (*cb)(void));

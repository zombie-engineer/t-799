#include <ili9341.h>
#include <stddef.h>
#include <ioreg.h>
#include <compiler.h>
#include <log.h>
#include <gpio.h>
#include <os_api.h>
#include <printf.h>
#include <spi.h>
#include <errcode.h>
#include <cpu.h>
#include <bcm2835_dma.h>
#include <kmalloc.h>
#include <common.h>
#include <mmu.h>
#include <memory_map.h>
#include <bitops.h>
#include "ili9341_command.h"

typedef enum {
  DISPLAY_STATE_UNKNOWN = 0,
  DISPLAY_STATE_CMD_STEP0,
  DISPLAY_STATE_DMA_PIXEL_DATA
} display_state_t;

typedef enum {
  ILI9341_NONSTOP_REFRESH_NONE = 0,
  ILI9341_NONSTOP_REFRESH_READY,
  ILI9341_NONSTOP_REFRESH_RUNNING,
  ILI9341_NONSTOP_REFRESH_PAUSED
} ili9341_nonstop_refresh_state_t;

#ifdef DISPLAY_MODE_PORTRAIT
#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 320
#else
#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 180
#endif

#ifdef DISPLAY_MODE_PORTRAIT
#define ACCESS_CONTROL_BYTE (ILI9341_CMD_MEM_ACCESS_CONTROL_BGR | \
  ILI9341_CMD_MEM_ACCESS_CONTROL_MX | \
  ILI9341_CMD_MEM_ACCESS_CONTROL_MY
#else
#define ACCESS_CONTROL_BYTE \
    ILI9341_CMD_MEM_ACCESS_CONTROL_BGR \
    | ILI9341_CMD_MEM_ACCESS_CONTROL_MX \
    | ILI9341_CMD_MEM_ACCESS_CONTROL_MY \
    | ILI9341_CMD_MEM_ACCESS_CONTROL_MV
#endif

struct ili9341_gpio {
  int pin_blk;
  int pin_reset;
  int dc;
  int pin_mosi;
  int pin_miso;
  int pin_sclk;
};

struct ili9341_5byte_cmd {
  uint8_t cmd;
  uint8_t data[4];
};

struct ili9341_spi_tasks {
  struct ili9341_5byte_cmd caset;
  struct ili9341_5byte_cmd paset;
  uint8_t cmdbyte_ramwr;

  struct spi_async_task spi_caset_cmd;
  struct spi_async_task spi_caset_data;
  struct spi_async_task spi_paset_cmd;
  struct spi_async_task spi_paset_data;
  struct spi_async_task spi_ramwr_cmd;
};

struct ili9341 {
  struct ili9341_gpio gpio;
  struct spi_device spi;
  struct list_head draw_tasks;

  bool transfer_done;

  int dma_ch_tx;
  int dma_ch_rx;
  int num_dma_xfer_done;
  struct ili9341_drawframe *drawframes;
  int num_drawframes;
  struct ili9341_per_frame_dma *dma_io_current;
  struct ili9341_spi_tasks *spi_tasks;
  uint32_t spi_header_max;
  uint32_t dma_rx_dst;
  ioreg32_t reg_addr_dc_clear;
  uint32_t reg_val_dc_clear;
  ioreg32_t reg_addr_dc_set;
  uint32_t reg_val_dc_set;
  uint32_t reg_addr_spi_fifo;
  display_state_t state;
};

#define BYTES_PER_PIXEL 3

#define BYTES_PER_FRAME(__width, __height) \
  ((__width) * (__height) * BYTES_PER_PIXEL)

/* 230400 */
#define NUM_BYTES_PER_FRAME BYTES_PER_FRAME(DISPLAY_HEIGHT, DISPLAY_WIDTH)

static struct ili9341 ili9341;

static ili9341_nonstop_refresh_state_t ili9341_nonstop_refresh_state
 = ILI9341_NONSTOP_REFRESH_NONE;

/*
 * Precalculated transfer of image via SPI and DMA:
 * Image dimentions are 320 X 240 X 3 bytes = 230400
 * Size of one DMA transfer is limited by how SPI is working in DMA mode.
 * When DMA enabled, SPI treats first word written to SPI FIFO
 * as a 16 bits of transfer length and 8 bits of lower byte of SPI_CS register
 * composed into 32bit word => Maximum size = 16 bits, 0xffff = 65535, we
 * want to have word aligned access for DMA, so we want any value aligned to 4
 * bytes > maximum 'good' transfer size is:
 * 0xffff & ~(4-1) = 0xffff & ~3 = 0xfffc = 65532
 * Number of 65532 byte transfers we need to transfer 230400 is:
 * roundup(230400 / 65532) = roundup(3.51584) = 4
 */
static inline int get_num_transfers(size_t length)
{
  return ROUND_UP_DIV(length, spi_get_max_transfer_size());
}

static inline int ili9341_get_parent_drawframe(
  struct ili9341_per_frame_dma *dma_io,
  struct ili9341_drawframe **out_drawframe,
  struct ili9341_spi_tasks **out_spi_task)
{
  int i, j;
  struct ili9341 *dev = &ili9341;
  struct ili9341_drawframe *drawframe;

  for (i = 0; i < dev->num_drawframes; ++i) {
    drawframe = &dev->drawframes[i];
    for (j = 0; j < drawframe->num_bufs; ++j) {
      if (&drawframe->bufs[j] == dma_io) {
        *out_drawframe = drawframe;
        *out_spi_task = &dev->spi_tasks[i];
        return 0;
      }
    }
  }

  return ERR_NOT_FOUND;
}

static void ili9341_dma_rx_done_irq(void)
{
  struct ili9341 *dev = &ili9341;
  struct ili9341_per_frame_dma *dma_io = dev->dma_io_current;
  struct spi_dma_xfer_cb *cbs;
  struct ili9341_drawframe *current_drawframe;
  struct ili9341_spi_tasks *current_spi_task;

  spi_dma_disable();

  dev->num_dma_xfer_done++;

  if (dev->num_dma_xfer_done == dma_io->num_transfers) {
    if (ili9341_get_parent_drawframe(dma_io, &current_drawframe,
      &current_spi_task)) {
      printf("PROBLEM3\n");
      asm volatile ("wfe");
    }

    if (current_drawframe->on_dma_done_irq)
      current_drawframe->on_dma_done_irq(dma_io);

    dev->dma_io_current = NULL;
    return;
  }

  cbs = &dma_io->cbs[dev->num_dma_xfer_done];
  bcm2835_dma_set_cb(dev->dma_ch_tx, cbs->spi_start);
  bcm2835_dma_set_cb(dev->dma_ch_rx, cbs->rx);

  spi_dma_enable();

  bcm2835_dma_activate(dev->dma_ch_rx);
  bcm2835_dma_activate(dev->dma_ch_tx);
}

static void ili9341_dma_tx_done_irq(void)
{
  printf("hello\r\n");
}

static void ili9341_write_command_byte(uint8_t cmd)
{
  struct ili9341 *dev = &ili9341;
  uint8_t bytestream_tx = cmd;
  *dev->reg_addr_dc_clear = dev->reg_val_dc_clear;
  spi_io_interrupt(&bytestream_tx, NULL, 1);
  *dev->reg_addr_dc_set = dev->reg_val_dc_set;
}

static void ili9341_write_data_bytes(const uint8_t *data, size_t count)
{
  spi_io_interrupt(data, NULL, count);
}

void ili9341_set_region_coords(uint16_t x0, uint16_t y0, uint16_t x1,
  uint16_t y1)
{
  uint8_t data_x[] = ILI9341_CASET_INIT_ARG(x0, x1);
  uint8_t data_y[] = ILI9341_PASET_INIT_ARG(y0, y1);

  ili9341_write_command_byte(ILI9341_CMD_CASET);
  ili9341_write_data_bytes(data_x, sizeof(data_x));
  ili9341_write_command_byte(ILI9341_CMD_PASET);
  ili9341_write_data_bytes(data_y, sizeof(data_y));
}

int ili9341_drawframe_set_irq(struct ili9341_drawframe *r,
  ili9341_on_dma_done_irq on_dma_done_irq)
{
  struct ili9341 *dev = &ili9341;

  if (ili9341_nonstop_refresh_state != ILI9341_NONSTOP_REFRESH_NONE) {
    os_log("WARN: tried to initialized nonstop refresh twice\r\n");
    return ERR_GENERIC;
  }

  r->on_dma_done_irq = on_dma_done_irq;

  ili9341_nonstop_refresh_state = ILI9341_NONSTOP_REFRESH_READY;
  dev->transfer_done = true;
  return SUCCESS;
}

static void ili9143_write_pixels_prep_done_isr(void)
{
  struct ili9341 *dev = &ili9341;

  spi_dma_enable();

  bcm2835_dma_activate(dev->dma_ch_rx);
  bcm2835_dma_activate(dev->dma_ch_tx);
}

static void ili9341_fill_screen_dma_async(struct ili9341_per_frame_dma *dma_io,
  struct ili9341_spi_tasks *t)
{
  struct ili9341 *dev = &ili9341;

  dev->num_dma_xfer_done = 0;
  dev->dma_io_current = dma_io;
  bcm2835_dma_set_cb(dev->dma_ch_tx, dma_io->cbs[0].spi_start);
  bcm2835_dma_set_cb(dev->dma_ch_rx, dma_io->cbs[0].rx);

  spi_io_async(&t->spi_caset_cmd, ili9143_write_pixels_prep_done_isr);
}

void OPTIMIZED ili9341_draw_dma_buf(struct ili9341_per_frame_dma *dma_io)
{
  struct ili9341_drawframe *drawframe;
  struct ili9341_spi_tasks *spi_task;

  if (ili9341_get_parent_drawframe(dma_io, &drawframe, &spi_task)) {
    printf("PROBLEM2\r\n");
    while (1) {
      asm volatile ("wfe");
    }
  }

  ili9341_fill_screen_dma_async(dma_io, spi_task);
}

static inline int ili9341_init_gpio(int gpio_blk, int gpio_dc, int gpio_reset)
{
  struct ili9341 *dev = &ili9341;
  struct ili9341_gpio *gpio = &dev->gpio;
  gpio->pin_mosi  = GPIO_PIN_10;
  gpio->pin_miso  = GPIO_PIN_9;
  gpio->pin_sclk  = GPIO_PIN_11;
  gpio_set_pin_function(gpio->pin_mosi, GPIO_FUNCTION_ALT_0);
  gpio_set_pin_function(gpio->pin_miso, GPIO_FUNCTION_ALT_0);
  gpio_set_pin_function(gpio->pin_sclk, GPIO_FUNCTION_ALT_0);

  gpio->pin_blk   = gpio_blk;
  gpio->dc = gpio_dc;
  gpio->pin_reset = gpio_reset;

  gpio_set_pin_function(gpio->pin_blk, GPIO_FUNCTION_OUTPUT);
  gpio_set_pin_function(gpio->pin_reset, GPIO_FUNCTION_OUTPUT);
  gpio_set_pin_function(gpio->dc, GPIO_FUNCTION_OUTPUT);
  gpio_set_pin_output(gpio->dc, true);
  gpio_set_pin_output(gpio->pin_blk, true);
  gpio_set_pin_output(gpio->pin_reset, false);
  gpio_get_reg32_addr(GPIO_REG_ADDR_CLR, gpio->dc, &dev->reg_addr_dc_clear,
    &dev->reg_val_dc_clear);
  gpio_get_reg32_addr(GPIO_REG_ADDR_SET, gpio->dc, &dev->reg_addr_dc_set,
    &dev->reg_val_dc_set);
  return SUCCESS;
}

static inline void ili9341_init_spi(int gpio_blk, int gpio_dc, int gpio_reset)
{
  struct ili9341 *dev = &ili9341;

  ioreg32_t reg_spi_fifo;
  ili9341_init_gpio(gpio_blk, gpio_dc, gpio_reset);
  spi_set_clk(8);
  spi_clear_rx_tx_fifo();
  spi_get_reg32_addr(SPI_REG_ADDR_DATA, &reg_spi_fifo);
  dev->reg_addr_spi_fifo = PERIPH_ADDR_TO_DMA(reg_spi_fifo);
}

static void ili9341_dma_chain_prep(struct ili9341_drawframe *drawframe,
  struct ili9341_per_frame_dma *dma_io)
{
  size_t num_transfers;
  size_t last_transfer_size;
  struct bcm2835_dma_request_param cb_tx_conf;
  struct bcm2835_dma_request_param cb_rx_conf;
  uint32_t *spi_word;
  uint32_t len;
  bool is_last_trans;

  struct ili9341 *dev = &ili9341;
  struct spi_dma_xfer_cb *cbs;
  uint32_t current_src = RAM_PHY_TO_BUS_UNCACHED(dma_io->buf);

  num_transfers = get_num_transfers(dma_io->buf_size);
  dma_io->cbs = kmalloc(sizeof(*dma_io->cbs) * num_transfers);

  last_transfer_size = dma_io->buf_size % spi_get_max_transfer_size();

  for (int i = 0; i < num_transfers; ++i) {
    cbs = &dma_io->cbs[i];
    cbs->spi_start = bcm2835_dma_reserve_cb();
    cbs->tx = bcm2835_dma_reserve_cb();
    cbs->rx = bcm2835_dma_reserve_cb();

    is_last_trans = i == num_transfers - 1;
    spi_word = is_last_trans ? &drawframe->spi_header_last
      : &dev->spi_header_max;

    len = is_last_trans ? last_transfer_size : spi_get_max_transfer_size();

    cb_tx_conf.dreq = DMA_DREQ_SPI_TX;
    cb_tx_conf.dreq_type = BCM2835_DMA_DREQ_TYPE_DST;
    cb_tx_conf.src = RAM_PHY_TO_BUS_UNCACHED(spi_word);
    cb_tx_conf.dst = dev->reg_addr_spi_fifo;
    cb_tx_conf.src_type = BCM2835_DMA_ENDPOINT_TYPE_NOINC;
    cb_tx_conf.dst_type = BCM2835_DMA_ENDPOINT_TYPE_NOINC;
    cb_tx_conf.len = 4;
    cb_tx_conf.enable_irq = false;
    bcm2835_dma_program_cb(&cb_tx_conf, cbs->spi_start);

    cb_tx_conf.dreq = DMA_DREQ_SPI_TX;
    cb_tx_conf.dreq_type = BCM2835_DMA_DREQ_TYPE_DST;
    cb_tx_conf.src = current_src;
    cb_tx_conf.dst = dev->reg_addr_spi_fifo;
    cb_tx_conf.src_type = BCM2835_DMA_ENDPOINT_TYPE_INCREMENT;
    cb_tx_conf.dst_type = BCM2835_DMA_ENDPOINT_TYPE_NOINC;
    cb_tx_conf.len = len;
    cb_tx_conf.enable_irq = false;
    bcm2835_dma_program_cb(&cb_tx_conf, cbs->tx);

    cb_rx_conf.dreq      = DMA_DREQ_SPI_RX;
    cb_rx_conf.dreq_type = BCM2835_DMA_DREQ_TYPE_SRC;
    cb_rx_conf.dst_type  = BCM2835_DMA_ENDPOINT_TYPE_NOINC;
    cb_rx_conf.src       = dev->reg_addr_spi_fifo;
    cb_rx_conf.dst       = RAM_PHY_TO_BUS_UNCACHED(&dev->dma_rx_dst);
    cb_rx_conf.src_type  = BCM2835_DMA_ENDPOINT_TYPE_NOINC;
    cb_rx_conf.len       = len;
    cb_rx_conf.enable_irq = true;

    bcm2835_dma_program_cb(&cb_rx_conf, cbs->rx);

    current_src += len;

    bcm2835_dma_link_cbs(cbs->spi_start, cbs->tx);
  }

  if (num_transfers > 1) {
    dcache_flush(&drawframe->spi_header_last,
      sizeof(drawframe->spi_header_last));
  }
}

static void ili9341_cmd_dc_clear_isr(void)
{
  struct ili9341 *dev = &ili9341;
  *dev->reg_addr_dc_clear = dev->reg_val_dc_clear;
}

static void ili9341_cmd_dc_set_isr(void)
{
  struct ili9341 *dev = &ili9341;
  *dev->reg_addr_dc_set = dev->reg_val_dc_set;
}

static inline void ili9341_5byte_cmd_init(struct ili9341_5byte_cmd *c,
  uint8_t cmd, uint8_t arg0, uint8_t arg1, uint8_t arg2, uint8_t arg3)
{
  c->cmd = cmd;
  c->data[0] = arg0;
  c->data[1] = arg1;
  c->data[2] = arg2;
  c->data[3] = arg3;
}

static void ili9341_spi_tasks_init(struct ili9341_spi_tasks *t,
  int x0, int y0, int x1, int y1)
{
  ili9341_5byte_cmd_init(&t->caset, ILI9341_CMD_CASET,
    BYTE_EXTRACT32(x0, 1), BYTE_EXTRACT32(x0, 0),
    BYTE_EXTRACT32(x1, 1), BYTE_EXTRACT32(x1, 0));

  ili9341_5byte_cmd_init(&t->paset, ILI9341_CMD_PASET,
    BYTE_EXTRACT32(y0, 1), BYTE_EXTRACT32(y0, 0),
    BYTE_EXTRACT32(y1, 1), BYTE_EXTRACT32(y1, 0));

  t->cmdbyte_ramwr = ILI9341_CMD_RAMWR;

  t->spi_caset_cmd.next = &t->spi_caset_data;
  t->spi_caset_cmd.io.rx = NULL;
  t->spi_caset_cmd.io.tx = &t->caset.cmd;
  t->spi_caset_cmd.io.num_bytes = 1;
  t->spi_caset_cmd.pre_cb_isr = ili9341_cmd_dc_clear_isr;
  t->spi_caset_cmd.post_cb_isr = ili9341_cmd_dc_set_isr;

  t->spi_caset_data.next = &t->spi_paset_cmd;
  t->spi_caset_data.io.rx = NULL;
  t->spi_caset_data.io.tx = t->caset.data;
  t->spi_caset_data.io.num_bytes = 4;
  t->spi_caset_data.pre_cb_isr = NULL;
  t->spi_caset_data.post_cb_isr = NULL;

  t->spi_paset_cmd.next = &t->spi_paset_data;
  t->spi_paset_cmd.io.rx = NULL;
  t->spi_paset_cmd.io.tx = &t->paset.cmd;
  t->spi_paset_cmd.io.num_bytes = 1;
  t->spi_paset_cmd.pre_cb_isr = ili9341_cmd_dc_clear_isr;
  t->spi_paset_cmd.post_cb_isr = ili9341_cmd_dc_set_isr;

  t->spi_paset_data.next = &t->spi_ramwr_cmd;
  t->spi_paset_data.io.rx = NULL;
  t->spi_paset_data.io.tx = t->paset.data;
  t->spi_paset_data.io.num_bytes = 4;
  t->spi_paset_data.pre_cb_isr = NULL;
  t->spi_paset_data.post_cb_isr = NULL;

  t->spi_ramwr_cmd.next = NULL;
  t->spi_ramwr_cmd.io.rx = NULL;
  t->spi_ramwr_cmd.io.tx = &t->cmdbyte_ramwr;
  t->spi_ramwr_cmd.io.num_bytes = 1;
  t->spi_ramwr_cmd.pre_cb_isr = ili9341_cmd_dc_clear_isr;
  t->spi_ramwr_cmd.post_cb_isr = ili9341_cmd_dc_set_isr;
}

static void __attribute__((optimize("O0"))) ili9341_drawframe_init(struct ili9341_drawframe *fr,
  struct ili9341_spi_tasks *t)
{
  int i;
  struct ili9341_per_frame_dma *dma_io;
  struct rectangle *rect = &fr->frame;
  size_t last_transfer_size;

  ili9341_spi_tasks_init(t, rect->pos.x, rect->pos.y,
    rect->pos.x + rect->size.x,
    rect->pos.y + rect->size.y);

  /*
   * SPI DMA transfer is split in chain of smaller transfers of size
   * MAX_BYTES_PER_TRANSFER.
   * Each transfer starts with a write to spi control reg, they are also done
   * by DMA, here they are called spi_header.
   * We need to program spi header values upfront.
   * If transfer size contains multiple MAX_BYTES_PER_TRANSFER, spi_header_max
   * is reused and its address is inserted into the chain of dma control blocks
   * DMA chain building logic will add per-region spi_header_last if required
   * by buffer_size, that is not evenly devisible by MAX SIZE
   *
   * Spi headers are first 4 bytes written to SPI FIFO to initiate SPI periph
   * for a specified size of DMA transaction, from datasheet bcm2835 10.6.3 DMA
   * 1 word to SPI FIFO: top 16 bits - transfer length, bottom 8 bits - control
   * register settings - [31:16 - transfer length, 15:8 - nothing, 7:0 -
   * control register first 8 bits]
   */
  if (fr->bufs[0].buf_size > spi_get_max_transfer_size()) {
    last_transfer_size = fr->bufs[0].buf_size % spi_get_max_transfer_size();
    fr->spi_header_last = spi_get_dma_word0(last_transfer_size);
  }

  fr->num_transfers = get_num_transfers(fr->bufs[0].buf_size);
  for (i = 0; i < fr->num_bufs; ++i) {
    dma_io = &fr->bufs[i];
    dma_io->num_transfers = fr->num_transfers;
    ili9341_dma_chain_prep(fr, dma_io);
  }
}

static int ili9341_setup_dma(struct ili9341_drawframe *drawframes,
  int num_drawframes)
{
  struct ili9341 *dev = &ili9341;
  size_t i;

  dev->dma_ch_rx = bcm2835_dma_request_channel();
  BUG_IF(dev->dma_ch_rx == -1, "Failed to request DMA channel for SPI RX");

  dev->dma_ch_tx = bcm2835_dma_request_channel();
  BUG_IF(dev->dma_ch_tx == -1, "Failed to request DMA channel for SPI TX");

  bcm2835_dma_reset(dev->dma_ch_tx);
  bcm2835_dma_reset(dev->dma_ch_rx);

  bcm2835_dma_set_irq_callback(dev->dma_ch_tx, ili9341_dma_tx_done_irq);
  bcm2835_dma_set_irq_callback(dev->dma_ch_rx, ili9341_dma_rx_done_irq);

  bcm2835_dma_irq_enable(dev->dma_ch_tx);
  bcm2835_dma_irq_enable(dev->dma_ch_rx);
  bcm2835_dma_enable(dev->dma_ch_tx);
  bcm2835_dma_enable(dev->dma_ch_rx);

  dev->spi_header_max = spi_get_dma_word0(spi_get_max_transfer_size());
  dcache_flush(&dev->spi_header_max, sizeof(dev->spi_header_max));

  dev->spi_tasks = kmalloc(sizeof(struct ili9341_spi_tasks) * num_drawframes);

  for (i = 0; i < num_drawframes; ++i) {
    ili9341_drawframe_init(&drawframes[i], &dev->spi_tasks[i]);
  }

  dev->drawframes = drawframes;
  dev->num_drawframes = num_drawframes;

  return SUCCESS;
}

static int ili9341_init_drawframes(struct ili9341_drawframe *drawframe,
  int num_drawframes)
{
  int err;

  err = ili9341_setup_dma(drawframe, num_drawframes);
  if (err != SUCCESS) {
    os_log("ili9341: failed to initialize DMA, err: %d\r\n", err);
    return err;
  }

  return SUCCESS;
}

int ili9341_init(int gpio_blk, int gpio_dc, int gpio_reset,
  struct ili9341_drawframe *drawframes, int num_drawframes)
{
  int err;
  struct ili9341 *dev = &ili9341;
  uint8_t data[8];

  INIT_LIST_HEAD(&dev->draw_tasks);

  ili9341_init_spi(gpio_blk, gpio_dc, gpio_reset);

  gpio_set_pin_output(ili9341.gpio.pin_reset, true);
  os_wait_ms(120);
  gpio_set_pin_output(ili9341.gpio.pin_reset, false);
  os_wait_ms(120);
  gpio_set_pin_output(ili9341.gpio.pin_reset, true);
  os_wait_ms(120);

  ili9341_write_command_byte(ILI9341_CMD_SOFT_RESET);
  os_wait_ms(5);
  ili9341_write_command_byte(ILI9341_CMD_DISPLAY_OFF);
  data[0] = ACCESS_CONTROL_BYTE;
  ili9341_write_command_byte(ILI9341_CMD_MEM_ACCESS_CONTROL);
  ili9341_write_data_bytes(data, 1);

  ili9341_write_command_byte(ILI9341_CMD_COLMOD);
  data[0] = 0x66;
  ili9341_write_data_bytes(data, 1);

  ili9341_write_command_byte(ILI9341_CMD_SLEEP_OUT);
  os_wait_ms(120);
  ili9341_write_command_byte(ILI9341_CMD_DISPLAY_ON);
  os_wait_ms(120);

  err = ili9341_init_drawframes(drawframes, num_drawframes);
  if (err != SUCCESS) {
    os_log("Failed to init ili9341 frames, err: %d", err);
    return err;
  }

  spi_clear_rx_tx_fifo();
  return SUCCESS;
}

size_t ili9341_get_frame_byte_size(int frame_width, int frame_height)
{
  return BYTES_PER_FRAME(frame_width, frame_height);
}

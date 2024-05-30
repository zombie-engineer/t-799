#include <ili9341.h>
#include <stddef.h>
#include <ioreg.h>
#include <compiler.h>
#include <log.h>
#include <gpio.h>
#include <os_api.h>
#include <spi.h>
#include <errcode.h>
#include <cpu.h>
#include <bcm2835_dma.h>
#include <kmalloc.h>
#include <common.h>
#include <mmu.h>
#include <memory_map.h>

#define ILI9341_CMD_SOFT_RESET   0x01
#define ILI9341_CMD_READ_ID      0x04
#define ILI9341_CMD_SLEEP_OUT    0x11
#define ILI9341_CMD_DISPLAY_OFF  0x28
#define ILI9341_CMD_DISPLAY_ON   0x29
#define ILI9341_CMD_SET_CURSOR_X 0x2a
#define ILI9341_CMD_SET_CURSOR_Y 0x2b
#define ILI9341_CMD_WRITE_PIXELS 0x2c
#define ILI9341_CMD_MEM_ACCESS_CONTROL 0x36
#define ILI9341_CMD_SET_PIXEL_FORMAT 0x3a
#define ILI9341_CMD_WRITE_MEMORY_CONTINUE 0x3c
#define ILI9341_CMD_POWER_CTL_A  0xcb
#define ILI9341_CMD_POWER_CTL_B  0xcf
#define ILI9341_CMD_TIMING_CTL_A 0xe8
#define ILI9341_CMD_TIMING_CTL_B 0xea
#define ILI9341_CMD_POWER_ON_SEQ 0xed
#define ILI9341_CMD_PUMP_RATIO   0xf7
#define ILI9341_CMD_POWER_CTL_1  0xc0
#define ILI9341_CMD_POWER_CTL_2  0xc1
#define ILI9341_CMD_VCOM_CTL_1   0xc5
#define ILI9341_CMD_VCOM_CTL_2   0xc7
#define ILI9341_CMD_FRAME_RATE_CTL 0xb1
#define ILI9341_CMD_BLANK_PORCH  0xb5
#define ILI9341_CMD_DISPL_FUNC   0xb6

/*
 * ILI9341 displays are able to update at any rate between 61Hz to up to
 *  119Hz. Default at power on is 70Hz.
 */
#define ILI9341_FRAMERATE_61_HZ 0x1F
#define ILI9341_FRAMERATE_63_HZ 0x1E
#define ILI9341_FRAMERATE_65_HZ 0x1D
#define ILI9341_FRAMERATE_68_HZ 0x1C
#define ILI9341_FRAMERATE_70_HZ 0x1B
#define ILI9341_FRAMERATE_73_HZ 0x1A
#define ILI9341_FRAMERATE_76_HZ 0x19
#define ILI9341_FRAMERATE_79_HZ 0x18
#define ILI9341_FRAMERATE_83_HZ 0x17
#define ILI9341_FRAMERATE_86_HZ 0x16
#define ILI9341_FRAMERATE_90_HZ 0x15
#define ILI9341_FRAMERATE_95_HZ 0x14
#define ILI9341_FRAMERATE_100_HZ 0x13
#define ILI9341_FRAMERATE_106_HZ 0x12
#define ILI9341_FRAMERATE_112_HZ 0x11
#define ILI9341_FRAMERATE_119_HZ 0x10
#define ILI9341_UPDATE_FRAMERATE ILI9341_FRAMERATE_119_HZ

#ifdef DISPLAY_MODE_PORTRAIT
#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 320
#else
#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 240
#endif

#define SPI_CS   ((volatile uint32_t *)0x3f204000)
#define SPI_FIFO ((volatile uint32_t *)0x3f204004)
#define SPI_CLK  ((volatile uint32_t *)0x3f204008)
#define SPI_DLEN ((volatile uint32_t *)0x3f20400c)
#define SPI_CS_CLEAR_TX (1 << 4)
#define SPI_CS_CLEAR_RX (1 << 5)
#define SPI_CS_CLEAR    (3 << 4)
#define SPI_CS_TA       (1 << 7)
#define SPI_CS_DMAEN    (1 << 8)
#define SPI_CS_ADCS     (1 << 11)
#define SPI_CS_DONE     (1 << 16)
#define SPI_CS_RXD      (1 << 17)
#define SPI_CS_TXD      (1 << 18)
#define SPI_CS_RXR      (1 << 19)
#define SPI_CS_RXF      (1 << 20)

#define SPI_CS_7E   0x7e204000
#define SPI_FIFO_7E 0x7e204004

struct ili9341_gpio {
  int pin_blk;
  int pin_reset;
  int pin_dc;
  int pin_mosi;
  int pin_miso;
  int pin_sclk;
};

#define BYTES_PER_PIXEL 3
#define MAX_BYTES_PER_TRANSFER 0xfff8

/* 230400 */
#define NUM_BYTES_PER_FRAME (DISPLAY_HEIGHT * DISPLAY_WIDTH * BYTES_PER_PIXEL)

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
#define NUM_DMA_TRANSFERS \
  ((NUM_BYTES_PER_FRAME + (MAX_BYTES_PER_TRANSFER - 1))\
    / MAX_BYTES_PER_TRANSFER)

struct ili9341 {
  struct ili9341_gpio gpio;
  struct spi_device spi;

  /* DMA control blocks */
  int tx_cbs[4];
  int rx_cbs[4];
  int header_cbs[4];
  int dummy_cbs[4];

  bool transfer_done;

  int dma_channel_idx_spi_tx;
  int dma_channel_idx_spi_rx;
  uint32_t *spi_dma_tx_headers;
  int last_transfer_idx;
  uint32_t current_buffer_handle;
};

static struct ili9341 ili9341;

static void (*ili9341_dma_done_cb_irq)(uint32_t) = NULL;

static void ili9341_dma_irq_callback_spi_rx(void)
{
  if (ili9341.last_transfer_idx == NUM_DMA_TRANSFERS - 1) {
    *(int *)0x3f204000 &= ~SPI_CS_DMAEN;
    *(int *)0x3f204000 |= SPI_CS_CLEAR;

    // printf("< [Done %08x]\r\n", ili9341.current_buffer_handle);

    if (ili9341_dma_done_cb_irq)
      ili9341_dma_done_cb_irq(ili9341.current_buffer_handle);
    ili9341.transfer_done = true;
    return;
  }

  ili9341.last_transfer_idx++;
  *(int *)0x3f204000 |= SPI_CS_CLEAR;

  bcm2835_dma_set_cb(ili9341.dma_channel_idx_spi_tx,
    ili9341.header_cbs[ili9341.last_transfer_idx]);

  bcm2835_dma_set_cb(ili9341.dma_channel_idx_spi_rx,
    ili9341.rx_cbs[ili9341.last_transfer_idx]);

  bcm2835_dma_activate(ili9341.dma_channel_idx_spi_rx);
  bcm2835_dma_activate(ili9341.dma_channel_idx_spi_tx);
}

static void ili9341_dma_irq_callback_spi_tx(void)
{
}

#define SPI_DC_SET() \
  ioreg32_write((ioreg32_t)0x3f20001c, 1 << ili9341.gpio.pin_dc)

#define SPI_DC_CLEAR() \
  ioreg32_write((ioreg32_t)0x3f200028, 1 << ili9341.gpio.pin_dc)

static void ili9341_cmd(char cmd)
{
  if(*SPI_CS & SPI_CS_RXD)
    *SPI_CS = SPI_CS_TA | SPI_CS_CLEAR_RX;

  SPI_DC_CLEAR();
  while(!(*SPI_CS & SPI_CS_TXD));
  *SPI_FIFO = cmd;
  while(!(*SPI_CS & SPI_CS_DONE));
  SPI_DC_SET();
}

#define SEND_CMD(__cmd) ili9341_cmd(__cmd)

#define SEND_CMD_DATA(__cmd, __data, __datalen) do { \
  char *__ptr = (char *)(__data);\
  char *__end = __ptr + __datalen;\
  uint32_t r;\
  SEND_CMD(__cmd);\
  while(__ptr < __end) {\
    char __d = *(__ptr++);\
    r = ioreg32_read(SPI_CS);\
    if (r & SPI_CS_RXD) \
      ioreg32_write(SPI_CS, SPI_CS_TA | SPI_CS_CLEAR_RX);\
    while(1) {\
      r = ioreg32_read(SPI_CS);\
      if (r & SPI_CS_TXD)\
        break;\
    }\
    ioreg32_write(SPI_FIFO, __d);\
  }\
  while(1) {\
    r = ioreg32_read(SPI_CS);\
    if (r & SPI_CS_DONE)\
      break;\
    if (r & SPI_CS_RXR) \
      ioreg32_write(SPI_CS, SPI_CS_TA | SPI_CS_CLEAR_RX);\
  }\
} while(0)

#define SEND_CMD_CHAR_REP(__cmd, __r, __g, __b, __reps) do { \
  uint32_t r;\
  int __rep = 0;\
  SEND_CMD(__cmd);\
  while(__rep++ < __reps) {\
    r = ioreg32_read(SPI_CS);\
    if (r & SPI_CS_RXD) \
      ioreg32_write(SPI_CS, SPI_CS_TA | SPI_CS_CLEAR_RX);\
    while(1) {\
      r = ioreg32_read(SPI_CS);\
      if (r & SPI_CS_TXD)\
        break;\
    }\
    ioreg32_write(SPI_FIFO, __r);\
    while(1) {\
      r = ioreg32_read(SPI_CS);\
      if (r & SPI_CS_TXD)\
        break;\
    }\
    ioreg32_write(SPI_FIFO, __g);\
    while(1) {\
      r = ioreg32_read(SPI_CS);\
      if (r & SPI_CS_TXD)\
        break;\
    }\
    ioreg32_write(SPI_FIFO, __b);\
  }\
  while(1) {\
    r = ioreg32_read(SPI_CS);\
    if (r & SPI_CS_DONE)\
      break;\
    if (r & SPI_CS_RXR) \
      ioreg32_write(SPI_CS, SPI_CS_TA | SPI_CS_CLEAR_RX);\
  }\
} while(0)

#define RECV_CMD_DATA(__cmd, __data, __datalen) do { \
  char *__ptr = (char *)(__data);\
  char *__end = __ptr + __datalen;\
  uint32_t r;\
  SEND_CMD(__cmd);\
  ioreg32_write(SPI_CS, SPI_CS_CLEAR_RX | SPI_CS_CLEAR_TX);\
  ioreg32_write(SPI_CS, SPI_CS_CLEAR_RX | SPI_CS_CLEAR_TX | SPI_CS_TA);\
  while(__ptr < __end) {\
    uint8_t c;\
    r = ioreg32_read(SPI_CS);\
    if (ioreg32_read(SPI_CS) & SPI_CS_TXD) {\
      ioreg32_write(SPI_FIFO, 0);\
      while (ioreg32_read(SPI_CS) & SPI_CS_DONE);\
      while (ioreg32_read(SPI_CS) & SPI_CS_RXD) {\
        c = ioreg32_read(SPI_FIFO);\
        *(__ptr++) = c;\
      }\
    }\
  }\
} while(0)

static inline void ili9341_set_region_coords(int gpio_pin_dc, uint16_t x0,
  uint16_t y0, uint16_t x1, uint16_t y1)
{
#define LO8(__v) (__v & 0xff)
#define HI8(__v) ((__v >> 8) & 0xff)
  char data_x[4] = { HI8(x0), LO8(x0), HI8(x1), LO8(x1) };
  char data_y[4] = { HI8(y0), LO8(y0), HI8(y1), LO8(y1) };
  SEND_CMD_DATA(ILI9341_CMD_SET_CURSOR_X, data_x, sizeof(data_x));
  SEND_CMD_DATA(ILI9341_CMD_SET_CURSOR_Y, data_y, sizeof(data_y));
#undef LO8
#undef HI8
}

#define WAIT_Y true
#define WAIT_N false

#define TI(__src_type, __dst_type, __dreq, __dreq_type, __wait)\
  DMA_TI_ADDR_TYPE_##__src_type,\
  DMA_TI_ADDR_TYPE_##__dst_type,\
  DMA_DREQ_##__dreq,\
  DMA_TI_DREQ_T_##__dreq_type,\
  __wait

/* MEM -> SPI FIFO */
#define TI_TX_PIPE TI(INC_Y, INC_N, SPI_TX, DST, WAIT_Y)
/* SPI FIFO -> MEM */
#define TI_RX_PIPE TI(INC_N, IGNOR, SPI_RX, SRC, WAIT_N)
/* MEM -> MEM */
#define TI_MM_PIPE TI(INC_Y, INC_Y, NONE, NONE, WAIT_Y)

/*
 * ili9341_fill_rect
 * gpio_pin_dc - used in SPI macro to send command
 * x0, x1 - range of fill from [x0, x1), x1 not included
 * x1, x2 - range of fill from [y0, y1), y1 not included
 * r,g,b  - obviously color components of fill
 *
 * Speed: optimized version is using 3 64-bit values to compress rgb inside of them
 * this way 3 stores of 64bit words substitute 8 * 3 = 24 char stores, which are slower
 * anyway even in 1:1 compare.
 * Counts are incremented 19200000 per second = 19.2MHz
 * Rates are:
 * - buffer fill with chars                 : 269449  (0.014  sec) (14 millisec)
 * - buffer fill with 64-bitwords           : 16817   (0.0008 sec) (0.800 millisec)
 * - buffer transfer via SPI CLK=34 no DLEN : 5414663 (0.2820 sec) (280 millisec)
 * - buffer transfer via SPI CLK=34         : 4813039 (0.2506 sec) (250 millisec)
 * - buffer transfer via SPI CLK=32         : 4529939 (0.2359 sec) (239 millisec)
 * - buffer transfer via SPI CLK=24         : 3397452 (0.1769 sec) (176 millisec)
 * - buffer transfer via SPI CLK=16         : 2501551 (0.1302 sec) (130 millisec)
 * - buffer transfer via SPI CLK=8          : 2194014 (0.1142 sec) (114 millisec) < best
 * - buffer transfer via SPI CLK=4          : 2448441 (0.1275 sec) (127 millisec)
 * DLEN = 2 gives a considerable optimization
 */

#define ili9341_fast_fill(__dst, __count, __v1, __v2, __v3) \
  asm volatile(\
    "mov x0, %0\n"\
    "to .req x0\n"\
    "mov x1, #3\n"\
    "mul x1, x1, %1\n"\
    "add x1, x0, x1, lsl 3\n"\
    "to_end .req x1\n"\
    "mov x2, %2\n"\
    "val1 .req x2\n"\
    "mov x3, %3\n"\
    "val2 .req x3\n"\
    "mov x4, %4\n"\
    "val3 .req x4\n"\
    "1:\n"\
    "stp val1, val2, [to], #16\n"\
    "str val3, [to], #8\n"\
    "cmp to, to_end\n"\
    "bne 1b\n"\
    ".unreq to\n"\
    ".unreq to_end\n"\
    ".unreq val1\n"\
    ".unreq val2\n"\
    ".unreq val3\n"\
    ::"r"(__dst), "r"(__count), "r"(__v1), "r"(__v2), "r"(__v3): "x0", "x1", "x2", "x3", "x4")

static char ili9341_canvas[DISPLAY_WIDTH * DISPLAY_HEIGHT * BYTES_PER_PIXEL] ALIGNED(64);

static void OPTIMIZED ili9341_fill_rect(int gpio_pin_dc, int x0, int y0,
  int x1, int y1, uint8_t r, uint8_t g, uint8_t b)
{
  int y, x;
  int local_width, local_height;
  uint8_t *c;
  uint64_t v1, v2, v3, iters;

  local_width = x1 - x0;
  local_height = y1 - y0;
  if (local_width * local_height > 8) {
#define BP(v, p) ((uint64_t)v << (p * 8))
    v1 = BP(r, 0) | BP(g, 1) | BP(b, 2) | BP(r, 3) | BP(g, 4) | BP(b, 5) | BP(r, 6) | BP(g, 7);
    v2 = BP(b, 0) | BP(r, 1) | BP(g, 2) | BP(b, 3) | BP(r, 4) | BP(g, 5) | BP(b, 6) | BP(r, 7);
    v3 = BP(g, 0) | BP(b, 1) | BP(r, 2) | BP(g, 3) | BP(b, 4) | BP(r, 5) | BP(g, 6) | BP(b, 7);
#undef BP
    iters = local_width * local_height / 8;
    ili9341_fast_fill(ili9341_canvas, iters, v1, v2, v3);
  } else {
    c = ili9341_canvas;
    for (x = 0; x < local_width; ++x) {
      for (y = 0; y < local_height; ++y) {
        c = ili9341_canvas + (y * local_width + x) * 3;
        *(c++) = r;
        *(c++) = g;
        *(c++) = b;
      }
    }
  }
  ili9341_set_region_coords(gpio_pin_dc, x0, y0, x1, y1);

  SEND_CMD_DATA(ILI9341_CMD_WRITE_PIXELS, ili9341_canvas,
    local_width * local_height * 3);
}

static void OPTIMIZED ili9341_fill_screen(int gpio_pin_dc)
{
  ili9341_fill_rect(gpio_pin_dc, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0,
    255, 0);
}

int line_y = 0;

static void draw_line(int y, uint8_t *data, uint8_t color)
{
  uint8_t *ptr = (uint8_t *)data;
  ptr += 320 * 3 * y;
  for (int i = 32; i < 64; ++i) {
    ptr[i * 3] = color;
    ptr[i * 3 + 1] = color;
  }
}

volatile int ili9341_use_dma = 0;
bool ili9341_first = false;
typedef enum {
  ILI9341_NONSTOP_REFRESH_NONE = 0,
  ILI9341_NONSTOP_REFRESH_READY,
  ILI9341_NONSTOP_REFRESH_RUNNING,
  ILI9341_NONSTOP_REFRESH_PAUSED
} ili9341_nonstop_refresh_state_t;

static ili9341_nonstop_refresh_state_t
ili9341_nonstop_refresh_state = ILI9341_NONSTOP_REFRESH_NONE;

static uint8_t *ili9341_nonstop_refresh_dma_buffer_a = NULL;
static uint8_t *ili9341_nonstop_refresh_dma_buffer_b = NULL;

int ili9341_nonstop_refresh_init(void (*dma_done_cb_irq)(uint32_t))
{
  if (ili9341_nonstop_refresh_state != ILI9341_NONSTOP_REFRESH_NONE) {
    printf("WARN: tried to initialized nonstop refresh twice\r\n");
    return ERR_GENERIC;
  }

  ili9341_set_region_coords(ili9341.gpio.pin_dc, 0, 0, DISPLAY_WIDTH,
    DISPLAY_HEIGHT);
  SEND_CMD(ILI9341_CMD_WRITE_PIXELS);

  ili9341_nonstop_refresh_dma_buffer_a = dma_alloc(NUM_BYTES_PER_FRAME);
  if (!ili9341_nonstop_refresh_dma_buffer_a) {
    printf("Failed to allocate DMA buffer for ili9341 nonstop refresh\r\n");
    return ERR_RESOURCE;
  }

  ili9341_nonstop_refresh_dma_buffer_b = dma_alloc(NUM_BYTES_PER_FRAME);
  if (!ili9341_nonstop_refresh_dma_buffer_b) {
    printf("Failed to allocate DMA buffer for ili9341 nonstop refresh\r\n");
    return ERR_RESOURCE;
  }

  ili9341_dma_done_cb_irq = dma_done_cb_irq;

  ili9341_nonstop_refresh_state = ILI9341_NONSTOP_REFRESH_READY;
  return SUCCESS;
}

int ili9341_nonstop_refresh_get_dma_buffer(uint8_t **dma_buf_a,
  uint8_t **dma_buf_b, size_t *sz)
{
  if (ili9341_nonstop_refresh_state == ILI9341_NONSTOP_REFRESH_NONE)
    return ERR_GENERIC;

  *dma_buf_a = ili9341_nonstop_refresh_dma_buffer_a;
  *dma_buf_b = ili9341_nonstop_refresh_dma_buffer_b;
  *sz = NUM_BYTES_PER_FRAME;
  return SUCCESS;
}

int ili9341_nonstop_refresh_start(void)
{
  ili9341.transfer_done = true;

  if (ili9341_nonstop_refresh_state != ILI9341_NONSTOP_REFRESH_READY) {
    printf("Failed to start nontstop refresh mode for ili9341,"
        " should be in READY state\r\n");
    return ERR_GENERIC;
  }
  
  for (size_t i = 0; i < NUM_DMA_TRANSFERS; ++i) {
    bcm2835_dma_update_cb_src(ili9341.tx_cbs[i],
      RAM_PHY_TO_BUS_UNCACHED(ili9341_nonstop_refresh_dma_buffer_a +
        i * MAX_BYTES_PER_TRANSFER));
  }

  return SUCCESS;
}

int OPTIMIZED ili9341_nonstop_refresh_poke(uint32_t handle)
{
  if (!ili9341.transfer_done) {
    // printf("** [Not accepted %08x]\r\n", handle);
    return ERR_GENERIC;
  }

  // printf("> [Accepted %08x]\r\n", handle);
  ili9341.current_buffer_handle = handle;
  ili9341.transfer_done = false;

#if 0
  for (int i = 0; i < 320*240; ++i) {
    uint8_t *ptr = ili9341_nonstop_refresh_dma_buffer + i * 2;
    uint8_t tmp;
    tmp = ptr[0];
    ptr[0] = ptr[1];
    ptr[1] = tmp;
  }
#endif

  *(int *)0x3f204000 |= SPI_CS_DMAEN | SPI_CS_ADCS | SPI_CS_CLEAR;
  bcm2835_dma_reset(ili9341.dma_channel_idx_spi_tx);
  bcm2835_dma_reset(ili9341.dma_channel_idx_spi_rx);

  ili9341.last_transfer_idx = 0;
  bcm2835_dma_set_cb(ili9341.dma_channel_idx_spi_tx, ili9341.header_cbs[0]);
  bcm2835_dma_set_cb(ili9341.dma_channel_idx_spi_rx, ili9341.rx_cbs[0]);

    /*
     * Observations:
     * - We only write to display, so naively we would only use one channel for
     *   TX DMA
     * - But RX fifo will be full rather quick and stall transmission
     * - That is why it is required to have second DMA channel to serve SPI RX
     * - If SPI RX TI has DST_IGNORE flag set - this will lead no LEN register
     *   not being decremented, and control block for RX will not be switched
     * - Activation of RX and TX channels is not done atomically, but RX will
     *   wait for TX, because only first CB write for TX will enable SPI_CS.TA
     */
  bcm2835_dma_activate(ili9341.dma_channel_idx_spi_rx);
  bcm2835_dma_activate(ili9341.dma_channel_idx_spi_tx);
  return SUCCESS;
}

void OPTIMIZED ili9341_draw_bitmap(const uint8_t *data, size_t data_sz,
  void (*dma_done_cb_irq)(uint32_t))
{
  size_t i;

  if (!ili9341.transfer_done)
    return;

  ili9341.transfer_done = false;
  ili9341_dma_done_cb_irq = dma_done_cb_irq;

  if (!ili9341_first) {
    ili9341_set_region_coords(ili9341.gpio.pin_dc, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (ili9341_use_dma) {
      SEND_CMD_DATA(ILI9341_CMD_WRITE_PIXELS, data, data_sz);
      return;
    }

    SEND_CMD(ILI9341_CMD_WRITE_PIXELS);
    ili9341_first = true;
  }

#if 1
  draw_line(line_y, (uint8_t *)data, 0xff);

  line_y += 10;
  if (line_y >= 240)
    line_y = 0;
#endif

  for (i = 0; i < NUM_DMA_TRANSFERS; ++i) {
    bcm2835_dma_update_cb_src(ili9341.tx_cbs[i],
      RAM_PHY_TO_BUS_UNCACHED(data + i * MAX_BYTES_PER_TRANSFER));
  }

  *(int *)0x3f204000 |= SPI_CS_DMAEN | SPI_CS_ADCS | SPI_CS_CLEAR;
  bcm2835_dma_reset(ili9341.dma_channel_idx_spi_tx);
  bcm2835_dma_reset(ili9341.dma_channel_idx_spi_rx);

  ili9341.last_transfer_idx = 0;
  bcm2835_dma_set_cb(ili9341.dma_channel_idx_spi_tx, ili9341.header_cbs[0]);
  bcm2835_dma_set_cb(ili9341.dma_channel_idx_spi_rx, ili9341.rx_cbs[0]);

    /*
     * Observations:
     * - We only write to display, so naively we would only use one channel for
     *   TX DMA
     * - But RX fifo will be full rather quick and stall transmission
     * - That is why it is required to have second DMA channel to serve SPI RX
     * - If SPI RX TI has DST_IGNORE flag set - this will lead no LEN register
     *   not being decremented, and control block for RX will not be switched
     * - Activation of RX and TX channels is not done atomically, but RX will
     *   wait for TX, because only first CB write for TX will enable SPI_CS.TA
     */
  bcm2835_dma_activate(ili9341.dma_channel_idx_spi_rx);
  bcm2835_dma_activate(ili9341.dma_channel_idx_spi_tx);
}

static inline int ili9341_init_gpio(int pin_blk, int pin_dc, int pin_reset)
{
  struct ili9341_gpio *gpio = &ili9341.gpio;
  gpio->pin_mosi  = GPIO_PIN_10;
  gpio->pin_miso  = GPIO_PIN_9;
  gpio->pin_sclk  = GPIO_PIN_11;
  gpio_set_pin_function(gpio->pin_mosi, GPIO_FUNCTION_ALT_0);
  gpio_set_pin_function(gpio->pin_miso, GPIO_FUNCTION_ALT_0);
  gpio_set_pin_function(gpio->pin_sclk, GPIO_FUNCTION_ALT_0);

  gpio->pin_blk   = pin_blk;
  gpio->pin_dc    = pin_dc;
  gpio->pin_reset = pin_reset;

  gpio_set_pin_function(gpio->pin_blk, GPIO_FUNCTION_OUTPUT);
  gpio_set_pin_function(gpio->pin_reset, GPIO_FUNCTION_OUTPUT);
  gpio_set_pin_function(gpio->pin_dc, GPIO_FUNCTION_OUTPUT);
  gpio_set_pin_output(gpio->pin_dc, true);
  gpio_set_pin_output(gpio->pin_blk, true);
  gpio_set_pin_output(gpio->pin_reset, false);
  return SUCCESS;
}

static inline void ili9341_init_transport(void)
{
  ili9341_init_gpio(19, 13, 6);
  *SPI_CS = SPI_CS_CLEAR_TX | SPI_CS_CLEAR_RX;
  *SPI_CLK = 8;
}

static void ili9341_setup_spi_dma_transfer(int transfer_idx, bool is_last)
{
  struct bcm2835_dma_request_param r = { 0 };

  size_t bytes_already = transfer_idx * MAX_BYTES_PER_TRANSFER;
  size_t bytes_left = NUM_BYTES_PER_FRAME - bytes_already;
  size_t transfer_size = MIN(MAX_BYTES_PER_TRANSFER, bytes_left);

  ili9341.spi_dma_tx_headers[transfer_idx] = (transfer_size << 16) | SPI_CS_TA;

  r.dreq      = DMA_DREQ_SPI_TX;
  r.dreq_type = BCM2835_DMA_DREQ_TYPE_DST;
  r.dst_type  = BCM2835_DMA_ENDPOINT_TYPE_NOINC;
  r.dst       = SPI_FIFO_7E;
  r.src_type  = BCM2835_DMA_ENDPOINT_TYPE_INCREMENT;
  r.src       = RAM_PHY_TO_BUS_UNCACHED(
    &ili9341.spi_dma_tx_headers[transfer_idx]);
  r.len       = 4;
  r.enable_irq = false;

  bcm2835_dma_program_cb(&r, ili9341.header_cbs[transfer_idx]);

  r.dreq      = DMA_DREQ_SPI_TX;
  r.dreq_type = BCM2835_DMA_DREQ_TYPE_DST;
  r.dst       = SPI_FIFO_7E;
  r.src_type  = BCM2835_DMA_ENDPOINT_TYPE_INCREMENT;
  r.src       = 0;
  r.dst_type  = BCM2835_DMA_ENDPOINT_TYPE_NOINC;
  r.len       = transfer_size;
  r.enable_irq = false;
  bcm2835_dma_program_cb(&r, ili9341.tx_cbs[transfer_idx]);

  r.dreq      = DMA_DREQ_SPI_RX;
  r.dreq_type = BCM2835_DMA_DREQ_TYPE_SRC;
  r.src       = SPI_FIFO_7E;
  r.dst       = RAM_PHY_TO_BUS_UNCACHED(ili9341_canvas);
  r.src_type  = BCM2835_DMA_ENDPOINT_TYPE_NOINC;
  r.dst_type  = BCM2835_DMA_ENDPOINT_TYPE_NOINC;
  r.len       = transfer_size;
  r.enable_irq = true;
  bcm2835_dma_program_cb(&r, ili9341.rx_cbs[transfer_idx]);

  bcm2835_dma_link_cbs(ili9341.header_cbs[transfer_idx],
    ili9341.tx_cbs[transfer_idx]);
}

static void ili9341_setup_dma_control_blocks(void)
{
  size_t i;

  ili9341.dma_channel_idx_spi_rx = bcm2835_dma_request_channel();
  BUG_IF(ili9341.dma_channel_idx_spi_rx == -1,
    "Failed to request DMA channel for SPI RX");

  ili9341.dma_channel_idx_spi_tx = bcm2835_dma_request_channel();
  BUG_IF(ili9341.dma_channel_idx_spi_tx == -1,
    "Failed to request DMA channel for SPI TX");

  ili9341.spi_dma_tx_headers = dma_alloc(
    MAX(sizeof(*ili9341.spi_dma_tx_headers) * (NUM_DMA_TRANSFERS + 1), 32));

  for (i = 0; i < NUM_DMA_TRANSFERS; ++i) {
    ili9341.header_cbs[i] = bcm2835_dma_reserve_cb();
    BUG_IF(ili9341.header_cbs[i] == -1, "SPI DMA cb allocation failed");
    ili9341.tx_cbs[i] = bcm2835_dma_reserve_cb();
    BUG_IF(ili9341.tx_cbs[i] == -1, "SPI DMA cb allocation failed");
    ili9341.rx_cbs[i] = bcm2835_dma_reserve_cb();
    BUG_IF(ili9341.rx_cbs[i] == -1, "SPI DMA cb allocation failed");
    ili9341.dummy_cbs[i] = bcm2835_dma_reserve_cb();
    BUG_IF(ili9341.dummy_cbs[i] == -1, "SPI DMA cb allocation failed");
  }

  for (int i = 0; i < NUM_DMA_TRANSFERS; ++i)
    ili9341_setup_spi_dma_transfer(i, i == NUM_DMA_TRANSFERS - 1);

  bcm2835_dma_reset(ili9341.dma_channel_idx_spi_tx);
  bcm2835_dma_reset(ili9341.dma_channel_idx_spi_rx);

  bcm2835_dma_set_irq_callback(ili9341.dma_channel_idx_spi_tx,
    ili9341_dma_irq_callback_spi_tx);

  bcm2835_dma_set_irq_callback(ili9341.dma_channel_idx_spi_rx,
    ili9341_dma_irq_callback_spi_rx);

  bcm2835_dma_irq_enable(ili9341.dma_channel_idx_spi_tx);
  bcm2835_dma_irq_enable(ili9341.dma_channel_idx_spi_rx);
  bcm2835_dma_enable(ili9341.dma_channel_idx_spi_tx);
  bcm2835_dma_enable(ili9341.dma_channel_idx_spi_rx);
}

void ili9341_init(void)
{
  char data[8];
  ili9341_init_transport();

  gpio_set_pin_output(ili9341.gpio.pin_reset, true);
  os_wait_ms(120);
  gpio_set_pin_output(ili9341.gpio.pin_reset, false);
  os_wait_ms(120);
  gpio_set_pin_output(ili9341.gpio.pin_reset, true);
  os_wait_ms(120);

  ioreg32_write(SPI_CS, SPI_CS_TA);

  SEND_CMD(ILI9341_CMD_SOFT_RESET);
  os_wait_ms(5);
  SEND_CMD(ILI9341_CMD_DISPLAY_OFF);

/* horizontal refresh direction */
#define ILI9341_CMD_MEM_ACCESS_CONTROL_MH  (1<<2)
/* pixel format default is bgr, with this flag it's rgb */
#define ILI9341_CMD_MEM_ACCESS_CONTROL_BGR (1<<3)
/* horizontal refresh direction */
#define ILI9341_CMD_MEM_ACCESS_CONTROL_ML  (1<<4)
/*
 * swap rows and columns default is PORTRAIT, this flags makes it ALBUM
 */
#define ILI9341_CMD_MEM_ACCESS_CONTROL_MV  (1<<5)
/* column address order */
#define ILI9341_CMD_MEM_ACCESS_CONTROL_MX  (1<<6)
/* row address order */
#define ILI9341_CMD_MEM_ACCESS_CONTROL_MY  (1<<7)
  data[0] = ILI9341_CMD_MEM_ACCESS_CONTROL_BGR
    | ILI9341_CMD_MEM_ACCESS_CONTROL_MX
    | ILI9341_CMD_MEM_ACCESS_CONTROL_MY
#ifndef DISPLAY_MODE_PORTRAIT
    | ILI9341_CMD_MEM_ACCESS_CONTROL_MV
#endif
  ;
  SEND_CMD_DATA(ILI9341_CMD_MEM_ACCESS_CONTROL, data, 1);

  // data[0] = 0b101 | (0b101 << 4);
  // SEND_CMD_DATA(ILI9341_CMD_SET_PIXEL_FORMAT, data, 1);

  SEND_CMD(ILI9341_CMD_SLEEP_OUT);
  os_wait_ms(120);
  SEND_CMD(ILI9341_CMD_DISPLAY_ON);
  os_wait_ms(120);
  ili9341_setup_dma_control_blocks();
  ili9341_fill_screen(ili9341.gpio.pin_dc);
}

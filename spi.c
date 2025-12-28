#include "spi.h"
#include <memory_map.h>
#include <ioreg.h>
#include <stringlib.h>
#include <common.h>
#include <gpio.h>
#include <errcode.h>
#include <bitops.h>
#include <io_flags.h>
#include <delay.h>
#include <event.h>
#include <printf.h>
#include <os_api.h>
#include <irq.h>
#include <bcm2835/bcm2835_ic.h>

#define MAX_BYTES_PER_TRANSFER 0xfff8
#define SPI_BASE ((unsigned long long)BCM2835_MEM_PERIPH_BASE + 0x00204000)

/*
 * Description of SPI0 CS reg at SPI0_BASE + 0x00
 * 1:0 Chip Select (00, 01, 10 - chips 0, 1, 2. 11 -reserved)
 * 2   Clock Phase
 * 3   Clock Polarity
 * 5:4 CLEAR FIFO clear
 * 6   Chip Select Polarity
 * 7   Transfer active
 * 8   DMAEN DMA Enable
 * 9   INTD Interrupt on Done
 * 10  INTR Interrupt on RXR
 * 11  ADCS Auto Deassert Chip select
 * 12  REN Read Enable
 * 13  LEN LoSSI enable
 * 14  Unused
 * 15  Unused
 * 16  Done transfer Done
 * 17  RX FIFO contains Data
 * 18  TX FIFO can accept Data
 * 19  RX FIFO needs Reading (full)
 * 20  FX FIFO Full
 * 21  Chip Select 0 Polarity
 * 22  Chip Select 1 Polarity
 * 23  Chip Select 2 Polarity
 * 24  Enable DMA mode in Lossi mode
 * 25  Enable Long data word in Lossi mode if DMA_LEN set
 * 31:26 Write as 0
 */

#define SPI_CS_CS       BITS(0, 3)
#define SPI_CS_CPHA     BIT(2)
#define SPI_CS_CPOL     BIT(3)
#define SPI_CS_CLEAR    BITS(4, 3)
#define SPI_CS_CSPOL    BIT(6)
#define SPI_CS_TA       BIT(7)
#define SPI_CS_DMAEN    BIT(8)
#define SPI_CS_INTD     BIT(9)
#define SPI_CS_INTR     BIT(10)
#define SPI_CS_ADCS     BIT(11)
#define SPI_CS_REN      BIT(12)
#define SPI_CS_LEN      BIT(13)
#define SPI_CS_DONE     BIT(16)
#define SPI_CS_RXD      BIT(17)
#define SPI_CS_TXD      BIT(18)
#define SPI_CS_RXR      BIT(19)
#define SPI_CS_RXF      BIT(20)
#define SPI_CS_CSPOL0   BIT(21)
#define SPI_CS_CSPOL1   BIT(22)
#define SPI_CS_CSPOL2   BIT(23)
#define SPI_CS_DMA_LEN  BIT(24)
#define SPI_CS_LEN_LONG BIT(25)

#define SPI_CS   ((ioreg32_t)(SPI_BASE + 0x00))
#define SPI_FIFO ((ioreg32_t)(SPI_BASE + 0x04))
#define SPI_CLK  ((ioreg32_t)(SPI_BASE + 0x08))
#define SPI_DLEN ((ioreg32_t)(SPI_BASE + 0x0c))

struct spi_async_tasks {
  struct spi_async_task *current;
  bool should_run_precb;
  struct spi_io current_io;
  void (*done_cb_isr)(void);
};

struct spi_gpio_pin_desc {
  int pin;
  int function;
  int default_function;
};

struct spi_gpio_group_desc {
  struct spi_gpio_pin_desc pins[8];
  int num_cs_pins;
};

#define PIN_DESC(__pin, __function, __default) \
  { .pin = __pin, \
    .function = GPIO_FUNCTION_ ## __function, \
    .default_function = GPIO_FUNCTION_ ## __default \
  }

static inline void spi_gpio_pin_desc_set(struct spi_gpio_pin_desc *d, int pin,
  int function, int default_function)
{
  d->pin = pin;
  d->function = function;
  d->default_function = default_function;
}

static inline int spi_gpio_group_desc_num_pins(
  const struct spi_gpio_group_desc *g)
{
  /* SCK, MOSI, MISO + some number of chip select pins */
  return 3 + g->num_cs_pins;
}

#define GPIO_PIN_SPI0_CS1  GPIO_PIN_7
#define GPIO_PIN_SPI0_CS0  GPIO_PIN_8
#define GPIO_PIN_SPI0_MISO GPIO_PIN_9
#define GPIO_PIN_SPI0_MOSI GPIO_PIN_10
#define GPIO_PIN_SPI0_SCK  GPIO_PIN_11

static const struct spi_gpio_group_desc spi0_gpio_desc = {
  .pins = {
    PIN_DESC(GPIO_PIN_SPI0_SCK, ALT_0, INPUT),
    PIN_DESC(GPIO_PIN_SPI0_MOSI, ALT_0, INPUT),
    PIN_DESC(GPIO_PIN_SPI0_MISO, ALT_0, INPUT),
    PIN_DESC(GPIO_PIN_SPI0_CS0, ALT_0, INPUT),
    PIN_DESC(GPIO_PIN_SPI0_CS1, ALT_0, INPUT),
    PIN_DESC(GPIO_PIN_NA, INPUT, INPUT),
  },
  .num_cs_pins = 2
};

struct spi_bitbang_state {
  bool pins_configured;
};

struct spi_bitbang_state spi_bb_state = { 0 };
struct spi_async_tasks spi_async_tasks = { 0 };

static struct spi_gpio_group_desc spi_bb_gpio_desc = { 0 };
// static bool spi_bb_gpio_desc_filled = false;

int spi_configure_bitbang(struct spi_device *d, int sck, int mosi,
  int miso, const int *cs, int num_cs_pins)
{
  struct spi_gpio_pin_desc *p = spi_bb_gpio_desc.pins;

  spi_gpio_pin_desc_set(p++, sck, GPIO_FUNCTION_OUTPUT, GPIO_FUNCTION_INPUT);
  spi_gpio_pin_desc_set(p++, mosi, GPIO_FUNCTION_OUTPUT, GPIO_FUNCTION_INPUT);
  spi_gpio_pin_desc_set(p++, miso, GPIO_FUNCTION_INPUT, GPIO_FUNCTION_INPUT);
  for (int i = 0; i < num_cs_pins; ++i)
    spi_gpio_pin_desc_set(p++, cs[i], GPIO_FUNCTION_OUTPUT, GPIO_FUNCTION_INPUT);
  spi_bb_gpio_desc.num_cs_pins = num_cs_pins;

  spi_bb_state.pins_configured = true;
  return SUCCESS;
}

int spi0_init(struct spi_device *d)
{
  int i;
  const struct spi_gpio_pin_desc *p;
  int num_pins = spi_gpio_group_desc_num_pins(&spi0_gpio_desc);

  if (!d)
    return ERR_GENERIC;

  if (d->initialized)
    return SUCCESS;

  for (i = 0; i < num_pins; ++i) {
    p = &spi0_gpio_desc.pins[i];
    gpio_set_pin_function(p->pin, p->function);
  }

  *SPI_CLK = 32;
  *SPI_CS |= SPI_CS_CLEAR;
  d->initialized = true;
  return SUCCESS;
}

int spi0_deinit(struct spi_device *d)
{
  int i;
  int num_pins = spi_gpio_group_desc_num_pins(&spi0_gpio_desc);

  if (!d)
    return ERR_INVAL;

  if (!d->initialized)
    return ERR_INVAL;

  for (i = 0; i < num_pins; ++i) {
    const struct spi_gpio_pin_desc *p = &spi0_gpio_desc.pins[i];
    gpio_set_pin_function(p->pin, p->default_function);
  }

  *SPI_CS = 0;
  *SPI_CLK = 0;

  d->initialized = false;
  return SUCCESS;
}

static inline bool spi0_transfer_active(void)
{
  return ioreg32_read(SPI_CS) & SPI_CS_TA;
}

static inline bool spi0_transfer_in_progress(void)
{
  bool transfer_active = *SPI_CS & SPI_CS_TA;
  bool transfer_done = *SPI_CS & SPI_CS_DONE;
  return transfer_active && !transfer_done;
}

static int spi0_start_transfer(struct spi_device *d, int io_flags,
  int slave_addr)
{
  if (spi0_transfer_active())
    return ERR_BUSY;

  d->io_flags = io_flags;
  *SPI_CS &= ~SPI_CS_REN;
  *SPI_CS |= SPI_CS_CLEAR;
  *SPI_CS |= SPI_CS_TA;

  return SUCCESS;
}

static bool spi0_write_possible(void)
{
  return *SPI_CS & SPI_CS_TXD;
}

static bool spi0_read_possible(void)
{
  return *SPI_CS & SPI_CS_RXD;
}

static int spi0_do_transfer(struct spi_device *d,
  const uint8_t *src,
  uint8_t *dst,
  size_t len,
  size_t *actual_len)
{
  size_t i;

  if (!d || !actual_len)
    return ERR_INVAL;

  if (d->io_flags & IO_FLAG_READ && !dst)
    return ERR_INVAL;

  if (d->io_flags & IO_FLAG_WRITE && !src)
    return ERR_INVAL;

  for (i = 0; i < len; ++i) {
    if (d->io_flags & IO_FLAG_READ)
      while(!spi0_read_possible());

    if (d->io_flags & IO_FLAG_WRITE)
      while(!spi0_write_possible());

    /* Write 1 byte to FIFO */
    if (d->io_flags & IO_FLAG_WRITE)
      *SPI_FIFO = src[i];

    /* Read 1 byte from FIFO */
    if (d->io_flags & IO_FLAG_READ)
      dst[i] = *SPI_FIFO;
    /* DONE - TX sent */
    while(spi0_transfer_in_progress());
  }
  *actual_len = len;
  return SUCCESS;
}

static int spi0_finish_transfer(struct spi_device *d)
{
  if (!spi0_transfer_active())
    return ERR_INVAL;

  while (spi0_transfer_in_progress());

  *SPI_CS |= SPI_CS_CLEAR;
  *SPI_CS &= ~SPI_CS_TA;

  return SUCCESS;
}

#define GPIO_DESC_PIN_SCK  0
#define GPIO_DESC_PIN_MOSI 1
#define GPIO_DESC_PIN_MISO 2
#define GPIO_DESC_PIN_CS0  3
#define GPIO_DESC_PIN_CS1  4
#define GPIO_DESC_PIN_CS2  5

#define SPI_BB_PIN_SCK \
  spi_bb_gpio_desc.pins[GPIO_DESC_PIN_SCK].pin

#define SPI_BB_PIN_MOSI \
  spi_bb_gpio_desc.pins[GPIO_DESC_PIN_MOSI].pin

#define SPI_BB_PIN_MISO \
  spi_bb_gpio_desc.pins[GPIO_DESC_PIN_MISO].pin

#define SPI_BB_PIN_CS0 \
  spi_bb_gpio_desc.pins[GPIO_DESC_PIN_CS0].pin

#define SPI_BB_PIN_CS1 \
  spi_bb_gpio_desc.pins[GPIO_DESC_PIN_CS1].pin

#define SPI_BB_PIN_CS2 \
  spi_bb_gpio_desc.pins[GPIO_DESC_PIN_CS2].pin

#define SPI_SCK_LOW() \
  gpio_set_pin_output(SPI_BB_PIN_SCK, 0)

#define SPI_SCK_HIGH() \
  gpio_set_pin_output(SPI_BB_PIN_SCK, 1)

#define SPI_MOSI_LOW() \
  gpio_set_pin_output(SPI_BB_PIN_MOSI, 0)

#define SPI_MOSI_HIGH() \
  gpio_set_pin_output(SPI_BB_PIN_MOSI, 1)

#define SPI_CS0_LOW() \
  gpio_set_pin_output(SPI_BB_PIN_CS0, 0)

#define SPI_CS0_HIGH() \
  gpio_set_pin_output(SPI_BB_PIN_CS0, 1)

#define SPI_CS1_LOW() \
  gpio_set_pin_output(SPI_BB_PIN_CS1, 0)

#define SPI_CS1_HIGH() \
  gpio_set_pin_output(SPI_BB_PIN_CS1, 1)

#define SPI_CS2_LOW() \
  gpio_set_pin_output(SPI_BB_PIN_CS2, 0)

#define SPI_CS2_HIGH() \
  gpio_set_pin_output(SPI_BB_PIN_CS2, 1)

int spi_bb_init(struct spi_device *d)
{
  int i;
  const struct spi_gpio_pin_desc *p;
  int num_pins = spi_gpio_group_desc_num_pins(&spi_bb_gpio_desc);

  if (!d)
    return ERR_INVAL;

  if (d->initialized)
    return SUCCESS;

  if (!spi_bb_state.pins_configured)
    return ERR_INVAL;

  for (i = 0; i < num_pins; ++i) {
    p = &spi_bb_gpio_desc.pins[i];
    gpio_set_pin_function(p->pin, p->function);
  }

  SPI_CS0_HIGH();
  SPI_MOSI_LOW();
  SPI_SCK_LOW();
  d->initialized = true;
  return SUCCESS;
}

int spi_bb_deinit(struct spi_device *d)
{
  int i;
  int num_pins = spi_gpio_group_desc_num_pins(&spi_bb_gpio_desc);

  if (!d)
    return ERR_INVAL;

  if (!d->initialized)
    return ERR_INVAL;

  for (i = 0; i < num_pins; ++i) {
    const struct spi_gpio_pin_desc *p = &spi_bb_gpio_desc.pins[i];
    gpio_set_pin_function(p->pin, p->default_function);
  }

  d->initialized = false;
  return SUCCESS;
}

static int spi_bb_start_transfer(struct spi_device *d, int io_flags,
  int slave_addr)
{
  if (!d->initialized)
    return ERR_INVAL;

  if (d->io_flags & IO_FLAG_READ)
    return ERR_INVAL;

  d->io_flags = io_flags;

  if (spi_bb_gpio_desc.num_cs_pins)
    SPI_CS0_LOW();

  return SUCCESS;
}

/*
 * Let's do 100Khz 
 * At this freq 1 clock period = 1/100000 sec = 1/100 ms = 10 microseconds
 * = 1.25e-3 ms
 * = 1.25 microseconds
 */
static void spi_bb_clock_byte(uint8_t b)
{
  for (int i = 7; i >= 0; i--) {
    if (b & (1 << i)) {
      SPI_MOSI_HIGH();
    } else {
      SPI_MOSI_LOW();
    }
    SPI_SCK_HIGH();
    delay_us(50);
    SPI_SCK_LOW();
    delay_us(50);
  }
}

static int spi_bb_do_transfer(struct spi_device *d,
  const uint8_t *src,
  uint8_t *dst,
  size_t len,
  size_t *actual_len)
{
  int i;

  if (!d || !actual_len)
    return ERR_INVAL;

  if (d->io_flags & IO_FLAG_READ)
    return ERR_INVAL;

  if (d->io_flags & IO_FLAG_WRITE && !src)
    return ERR_INVAL;

  for (i = 0; i < len; ++i) {
    spi_bb_clock_byte(src[i]);
  }
  *actual_len = len;
  return SUCCESS;
}

static int spi_bb_finish_transfer(struct spi_device *d)
{
  if (spi_bb_gpio_desc.num_cs_pins)
    SPI_CS0_HIGH();
  return SUCCESS;
}

static const struct spi_fns spi0_fns = {
  .init = spi0_init,
  .deinit = spi0_deinit,
  .start_transfer = spi0_start_transfer,
  .do_transfer = spi0_do_transfer,
  .finish_transfer = spi0_finish_transfer
};

static struct spi_device spi0_dev = {
  .fns = &spi0_fns,
  .initialized = false
};

static const struct spi_fns spi_bb_fns = {
  .init = spi_bb_init,
  .deinit = spi_bb_deinit,
  .start_transfer = spi_bb_start_transfer,
  .do_transfer = spi_bb_do_transfer,
  .finish_transfer = spi_bb_finish_transfer
};

static struct spi_device spi_bb_dev = {
  .fns = &spi_bb_fns,
  .initialized = false
};

struct spi_device *spi_get_device(spi_device_id_t device_id)
{
  if (device_id == SPI_DEVICE_ID_SPI_0)
    return &spi0_dev;
  if (device_id == SPI_DEVICE_ID_BITBANG)
    return &spi_bb_dev;
  return NULL;
}

bool spi_get_reg32_addr(spi_reg_type_t t, ioreg32_t *out)
{
  if (t == SPI_REG_ADDR_CTRL) {
    *out = SPI_CS;
    return true;
  }

  if (t == SPI_REG_ADDR_DATA) {
    *out = SPI_FIFO;
    return true;
  }

  return false;
}

struct spi_io_nonasync {
  struct spi_io io;
  struct event ev;
};

struct spi_io_nonasync spi_io_nonasync = { 0 };

struct spi_io *spi_io_current = NULL;

static inline void spi_transfer_done_async_isr(uint32_t cs)
{
  struct spi_async_task *current;

  cs &= ~SPI_CS_TA;
  ioreg32_write(SPI_CS, cs);

  current = spi_async_tasks.current;
  if (current->post_cb_isr)
    current->post_cb_isr();

  current = current->next;
  spi_async_tasks.current = current;
  if (!current) {
    /* Async task list is completed */

    cs &= ~(SPI_CS_INTD | SPI_CS_INTR);
    ioreg32_write(SPI_CS, cs);
    if (spi_async_tasks.done_cb_isr) {
      spi_async_tasks.done_cb_isr();
      spi_async_tasks.done_cb_isr = NULL;
    }
    return;
  }

  /* Start next async task */
  spi_async_tasks.should_run_precb = current->pre_cb_isr != NULL;
  *spi_io_current = current->io;

  cs = ioreg32_read(SPI_CS);
  cs |= SPI_CS_TA;
  ioreg32_write(SPI_CS, cs);
}

static inline void OPTIMIZED spi_transfer_done_nonasync_isr(uint32_t cs)
{
  cs &= ~(SPI_CS_INTD | SPI_CS_INTR | SPI_CS_TA);
  ioreg32_write(SPI_CS, cs);
  os_event_notify_isr(&spi_io_nonasync.ev);
}

static inline uint32_t OPTIMIZED spi_isr_do(uint32_t *out_cs)
{
  int i;
  uint32_t intmask;
  uint32_t rx;
  int max_bytes;
  uint32_t cs = ioreg32_read(SPI_CS);
  struct spi_io *io = spi_io_current;
  intmask = SPI_CS_DONE | SPI_CS_RXR;

  if ((cs & intmask) == intmask) {
    printf("cs:%08x\r\n");
    while(1);
  }
  else if ((cs & intmask) == 0) {
    printf("cs:%08x\r\n");
    while(1);
  }

  if (cs & SPI_CS_DONE) {
    max_bytes = MIN(16, io->num_bytes);
    if (max_bytes == 0) {
      *out_cs = cs;
      return true;
    }
  }
  else if (cs & SPI_CS_RXR)
    max_bytes = MIN(12, io->num_bytes);
  else
    max_bytes = 0;

  for (i = 0; i < max_bytes; ++i) {
    ioreg32_write(SPI_FIFO, *io->tx);
    io->tx++;
    rx = ioreg32_read(SPI_FIFO);

    if (io->rx) {
      *io->rx = (uint8_t)rx;
      io->rx++;
    }

    io->num_bytes--;
    cs = ioreg32_read(SPI_CS);
    if (!(cs & SPI_CS_TXD))
      break;
  }
  *out_cs = cs;
  return false;
}

void spi_isr(void)
{
  struct spi_async_task *async;
  uint32_t cs;

  struct spi_io *io = spi_io_current;

  async = (io == &spi_async_tasks.current_io) ? spi_async_tasks.current : NULL;

  if (async) {
    if (async->pre_cb_isr && spi_async_tasks.should_run_precb) {
      spi_async_tasks.should_run_precb = false;
      async->pre_cb_isr();
    }
  }

  /* Not done yet, wait until DONE */
  if (!spi_isr_do(&cs)) {
    return;
  }

  /* Transfer is done */
  if (async)
    spi_transfer_done_async_isr(cs);
  else
    spi_transfer_done_nonasync_isr(cs);
}

static inline void OPTIMIZED spi_io_start(struct spi_io *io,
  const uint8_t *bytestream_tx, uint8_t *bytestream_rx, size_t count)
{
  uint32_t cs;

  io->num_bytes = count;
  io->tx = bytestream_tx;
  io->rx = bytestream_rx;
  spi_io_current = io;

  cs = ioreg32_read(SPI_CS);
  cs |= SPI_CS_INTD | SPI_CS_INTR | SPI_CS_TA;
  ioreg32_write(SPI_CS, cs);
}

void OPTIMIZED spi_io_interrupt(const uint8_t *bytestream_tx,
  uint8_t *bytestream_rx, size_t count)
{
  spi_io_start(&spi_io_nonasync.io, bytestream_tx, bytestream_rx, count);
  os_event_wait(&spi_io_nonasync.ev);
  os_event_clear(&spi_io_nonasync.ev);
}

void spi_io_async(struct spi_async_task *tasks, void (*done_cb_isr)(void))
{
  struct spi_async_task *t = tasks;
  spi_async_tasks.current = t;
  spi_async_tasks.should_run_precb = t->pre_cb_isr != NULL;
  spi_async_tasks.done_cb_isr = done_cb_isr;

  spi_io_start(&spi_async_tasks.current_io, t->io.tx, t->io.rx,
    t->io.num_bytes);
}

void spi_init(void)
{
  os_event_init(&spi_io_nonasync.ev);
  bcm2835_ic_enable_irq(BCM2835_IRQNR_SPI);
  irq_set(BCM2835_IRQNR_SPI, spi_isr);
  while (ioreg32_read(SPI_CS) & SPI_CS_RXD)
    ioreg32_read(SPI_FIFO);

  ioreg32_write(SPI_CS, 0);
}

void spi_reset_for_dma(void)
{
  ioreg32_write(SPI_CS, SPI_CS_DMAEN | SPI_CS_ADCS | SPI_CS_CLEAR);
}

void spi_dma_enable(void)
{
  uint32_t cs = ioreg32_read(SPI_CS);
  cs |= SPI_CS_DMAEN | SPI_CS_ADCS | SPI_CS_CLEAR;
  ioreg32_write(SPI_CS, cs);
}

void spi_dma_disable(void)
{
  uint32_t cs = ioreg32_read(SPI_CS);
  cs &= ~SPI_CS_DMAEN;
  cs |= SPI_CS_CLEAR;
  ioreg32_write(SPI_CS, cs);
}

void spi_clear_rx_tx_fifo(void)
{
  uint32_t cs = ioreg32_read(SPI_CS);
  cs |= SPI_CS_CLEAR;
  ioreg32_write(SPI_CS, cs);
}

uint32_t spi_get_max_transfer_size(void)
{
  return MAX_BYTES_PER_TRANSFER;
}

void spi_set_clk(uint32_t clk)
{
  *SPI_CLK = clk;
}

uint32_t spi_get_dma_word0(uint16_t transfer_size)
{
  return (transfer_size << 16) | SPI_CS_TA;
}


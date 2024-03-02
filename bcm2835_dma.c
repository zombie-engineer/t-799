#include <bcm2835_dma.h>
#include <bitmap.h>
#include <memory.h>
#include <errcode.h>
#include <common.h>
#include <kmalloc.h>
#include <bcm2835/bcm2835_ic.h>
#include <irq.h>
#include "bcm2835_dma_regs.h"

#define BCM2835_DMA_NUM_SCBS 32
#define BCM2835_DMA_NUM_CHANNELS 12

#define BCM2835_DMA_MAX_PER_CHANNEL_IRQ_CBS 4

struct bcm2835_dma_cb {
  uint32_t transfer_info;
  uint32_t source_addr;
  uint32_t dest_addr;
  uint32_t transfer_len;
  uint32_t mode_2d_stride;
  uint32_t next_cb_addr;
  uint32_t res0;
  uint32_t res1;
};

typedef void (*dma_irq_cb)(void);

struct bcm2835_dma_per_chan_irq_cbs
{
  dma_irq_cb cbs[BCM2835_DMA_MAX_PER_CHANNEL_IRQ_CBS];
};

struct bcm2835_dma {
  void *cb_area;
  struct bcm2835_dma_cb *cbs;
  struct bitmap cb_bitmap;
  struct bcm2835_dma_per_chan_irq_cbs irq_cbs[BCM2835_DMA_NUM_CHANNELS];

  int num_dma_irqs[BCM2835_DMA_NUM_CHANNELS];
  int num_read_errors[BCM2835_DMA_NUM_CHANNELS];
  int num_fifo_errors[BCM2835_DMA_NUM_CHANNELS];
  int num_read_last_not_set_errors[BCM2835_DMA_NUM_CHANNELS];
};

static uint64_t bcm2835_dma_cb_bitmap[BITMAP_NUM_WORDS(BCM2835_DMA_NUM_SCBS)];

static struct bcm2835_dma bcm2835_dma = { 0 };

static inline void bcm2835_dma_irq_handler_common(int channel)
{
  struct bcm2835_dma_per_chan_irq_cbs *cbs;
  size_t i;

  *DMA_CS(channel) |= DMA_CS_INT;

  bcm2835_dma.num_dma_irqs[channel]++;

  if (*DMA_DEBUG(channel) & DMA_DEBUG_READ_LAST_NOT_SET_ERROR)
    bcm2835_dma.num_read_last_not_set_errors[channel]++;

  if (*DMA_DEBUG(channel) & DMA_DEBUG_FIFO_ERROR)
    bcm2835_dma.num_fifo_errors[channel]++;

  if (*DMA_DEBUG(channel) & DMA_DEBUG_READ_ERROR)
    bcm2835_dma.num_read_errors[channel]++;

  cbs = &bcm2835_dma.irq_cbs[channel];
  for (i = 0; i < ARRAY_SIZE(cbs->cbs); ++i) {
    if (cbs->cbs[i])
      cbs->cbs[i]();
  }
}

#define BCM2835_DMA_IRQ_HANDLER(__channel) \
  static void bcm2835_dma_irq_handler_ch_## __channel(void) \
  { \
    bcm2835_dma_irq_handler_common(__channel); \
  }

BCM2835_DMA_IRQ_HANDLER(0);
BCM2835_DMA_IRQ_HANDLER(1);
BCM2835_DMA_IRQ_HANDLER(2);
BCM2835_DMA_IRQ_HANDLER(3);
BCM2835_DMA_IRQ_HANDLER(4);
BCM2835_DMA_IRQ_HANDLER(5);
BCM2835_DMA_IRQ_HANDLER(6);
BCM2835_DMA_IRQ_HANDLER(7);
BCM2835_DMA_IRQ_HANDLER(8);
BCM2835_DMA_IRQ_HANDLER(9);
BCM2835_DMA_IRQ_HANDLER(10);
BCM2835_DMA_IRQ_HANDLER(11);
BCM2835_DMA_IRQ_HANDLER(12);

static inline struct bcm2835_dma_cb *bcm2835_dma_cb_alloc(void)
{
  int bitmap_idx;
  struct bcm2835_dma_cb *cb = NULL;
  bitmap_idx = bitmap_set_next_free(&bcm2835_dma.cb_bitmap);
  if (bitmap_idx != -1)
    cb = &bcm2835_dma.cbs[bitmap_idx];

  return cb;
}

static inline void bcm2835_dma_cb_free(struct bcm2835_dma_cb *cb)
{
  struct bcm2835_dma_cb *cb_first = bcm2835_dma.cbs;
  struct bcm2835_dma_cb *cb_end = cb_first + bcm2835_dma.cb_bitmap.num_entries;
  int bitmap_idx;

  BUG_IF((uint64_t)cb & 0xff || cb < cb_first || cb >= cb_end,
    "Trying to free invalid cb");

  bitmap_idx = cb - cb_first;
  bitmap_clear_entry(&bcm2835_dma.cb_bitmap, bitmap_idx);
}

static inline uint32_t bcmd2835_dma_gen_ti(
  int dreq,
  bcm2835_dma_dreq_type_t dreq_type,
  bcm2835_dma_endpoint_type_t src_type,
  bcm2835_dma_endpoint_type_t dst_type,
  bool wait_resp,
  bool enable_irq)
{
  uint32_t value = 0;

  if (src_type == BCM2835_DMA_ENDPOINT_TYPE_INCREMENT)
    value |= DMA_TI_SRC_INC;
  else if (src_type != BCM2835_DMA_ENDPOINT_TYPE_NOINC)
    value |= DMA_TI_SRC_IGNORE;

  if (dst_type == BCM2835_DMA_ENDPOINT_TYPE_INCREMENT)
    value |= DMA_TI_DEST_INC;
  else if (dst_type != BCM2835_DMA_ENDPOINT_TYPE_NOINC)
    value |= DMA_TI_DEST_IGNORE;

  if (dreq_type == BCM2835_DMA_DREQ_TYPE_SRC)
    value |= DMA_TI_SRC_DREQ;
  else if (dreq_type == BCM2835_DMA_DREQ_TYPE_DST)
    value |= DMA_TI_DEST_DREQ;

  value |= DMA_TI_PERMAP(dreq);
  value |= DMA_TI_NO_WIDE_BURSTS;
  if (wait_resp)
    value |= DMA_TI_WAIT_RESP;

  if (enable_irq)
    value |= DMA_TI_INTEN;

  return value;
}

bool bcm2835_dma_link_cbs(int cb_handle, int cb_handle_next)
{
  struct bcm2835_dma_cb *cb, *cb_next;

  if (cb_handle == -1
    || cb_handle >= BCM2835_DMA_NUM_SCBS
    || cb_handle_next == -1
    || cb_handle_next >= BCM2835_DMA_NUM_SCBS)
    return false;

  cb = &bcm2835_dma.cbs[cb_handle];
  cb_next = &bcm2835_dma.cbs[cb_handle_next];
  cb->next_cb_addr = cb_next ? RAM_PHY_TO_BUS_UNCACHED(cb_next) : 0;
  return true;
}

bool bcm2835_dma_program_cb(const struct bcm2835_dma_request_param *p,
  int cb_handle)
{
  struct bcm2835_dma_cb *cb;

  if (cb_handle == -1 || cb_handle >= BCM2835_DMA_NUM_SCBS)
    return false;

  cb = &bcm2835_dma.cbs[cb_handle];
  cb->transfer_info = bcmd2835_dma_gen_ti(
    p->dreq,
    p->dreq_type,
    p->src_type,
    p->dst_type,
    true,
    p->enable_irq);

  cb->source_addr = p->src;
  cb->dest_addr = p->dst;
  cb->transfer_len = p->len;
  return true;
}

static void bcm2835_dma_request(const struct bcm2835_dma_request_param *p,
  struct bcm2835_dma_cb **next_cb)
{
  struct bcm2835_dma_cb *cb = bcm2835_dma_cb_alloc();
  memset(cb, 0, sizeof(*cb));
  cb->transfer_info = bcmd2835_dma_gen_ti(
    p->dreq,
    p->dreq_type,
    p->src_type,
    p->dst_type,
    true,
    p->enable_irq);

  cb->source_addr = p->src;
  cb->dest_addr = p->dst;
  cb->transfer_len = p->len;
  cb->mode_2d_stride = 0;
  cb->next_cb_addr = *next_cb ? RAM_PHY_TO_BUS_UNCACHED(*next_cb) : 0;
  *next_cb = cb;
}

#if 0
int bcm2835_dma_requests(const struct bcm2835_dma_request_param *p, size_t n)
{
  size_t i;
  struct bcm2835_dma_cb *next_cb = NULL;

  for (i = n; i > 0; --i)
    bcm2835_dma_request(&p[i - 1], &next_cb);

  *DMA_CONBLK_AD(p->channel) = RAM_PHY_TO_BUS_UNCACHED(next_cb);
}
#endif

void bcm2835_dma_enable(int channel)
{
  *DMA_ENABLE |= 1 << channel;
}

void bcm2835_dma_activate(int channel)
{
  *DMA_CS(channel) |= DMA_CS_ACTIVE;
}

void bcm2835_dma_reset(int channel)
{
  *DMA_CS(channel) |= DMA_CS_RESET;
}

void bcm2835_dma_poll(int channel)
{
  while(((*DMA_CS(channel)) & DMA_CS_END) == 0);
}

void bcm2835_dma_set_cb(int channel, int cb_handle)
{
  uint64_t cb_addr = (uint64_t)&bcm2835_dma.cbs[cb_handle];
  *DMA_CONBLK_AD(channel) = RAM_PHY_TO_BUS_UNCACHED(cb_addr);
}

int bcm2835_dma_reserve_cb(void)
{
  return bitmap_set_next_free(&bcm2835_dma.cb_bitmap);
}

bool bcm2835_dma_set_irq_callback(int channel, void (*cb)(void))
{
  size_t i;
  struct bcm2835_dma_per_chan_irq_cbs *cbs;

  if (channel >= ARRAY_SIZE(bcm2835_dma.irq_cbs))
    return false;

  cbs = &bcm2835_dma.irq_cbs[channel];
  for (i = 0; i < ARRAY_SIZE(cbs->cbs); ++i)
  {
    if (!cbs->cbs[i]) {
      cbs->cbs[i] = cb;
      return true;
    }
  }
  return false;
}

void bcm2835_dma_irq_enable(int channel)
{
  if (channel > BCM2835_DMA_NUM_CHANNELS)
    return;

  bcm2835_ic_enable_irq(BCM2835_IRQNR_DMA_0 + channel);
}

void bcm2835_dma_init(void)
{
  size_t total_size = 256 * (BCM2835_DMA_NUM_SCBS + 1);
  uint64_t addr;

  bcm2835_dma.cb_area = dma_alloc(total_size);
  if (!bcm2835_dma.cb_area)
    return;

  addr = ((uint64_t)bcm2835_dma.cb_area + 0xff - 1) & ~0xfful;
  total_size -= addr - (uint64_t)bcm2835_dma.cb_area;

  bcm2835_dma.cbs = (void *)addr;
  bcm2835_dma.cb_bitmap.data = bcm2835_dma_cb_bitmap;
  bcm2835_dma.cb_bitmap.num_entries = BCM2835_DMA_NUM_SCBS;
  bitmap_clear_all(&bcm2835_dma.cb_bitmap);

  irq_set(BCM2835_IRQNR_DMA_0, bcm2835_dma_irq_handler_ch_0);
  irq_set(BCM2835_IRQNR_DMA_1, bcm2835_dma_irq_handler_ch_1);
}

#include <drivers/dma/dma_bcm2835.h>
#include <bitmap.h>
#include <errcode.h>
#include <common.h>
#include <kmalloc.h>
#include <printf.h>
#include <drivers/intc/intc_bcm2835.h>
#include <irq.h>
#include <logger.h>
#include "dma_bcm2835_regs.h"

#define BCM2835_DMA_NUM_SCBS 256
#define BCM2835_DMA_NUM_CHANNELS 12

#define BCM2835_DMA_MAX_PER_CHANNEL_IRQ_CBS 4
#define DMA_CB0_PADDR  RAM_PHY_TO_BUS_UNCACHED(bcm2835_dma.cbs)

#undef BCM2835_DMA_ENABLE_LOG
#if BCM2835_DMA_ENABLE_LOG
#define BCM2835_DMA_LOG(__fmt, ...) os_log(__fmt "\r\n", ##__VA_ARGS__)
#else
#define BCM2835_DMA_LOG(__fmt, ...)
#endif

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
  struct bitmap channel_bitmap;
  uint64_t channel_bitmap_data;

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

static inline int  bcm2835_dma_cb_addr_to_handle(struct bcm2835_dma_cb *cb)
{
  struct bcm2835_dma_cb *cb_first = bcm2835_dma.cbs;
  struct bcm2835_dma_cb *cb_end = cb_first + bcm2835_dma.cb_bitmap.num_entries;

  BUG_IF((uint64_t)cb & 0xff || cb < cb_first || cb >= cb_end,
    "Trying to free invalid cb");

  return cb - cb_first;
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
  cb_next = cb_handle_next == CB_HANDLE_NEXT_NONE ? NULL :
    &bcm2835_dma.cbs[cb_handle_next];

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
  cb->next_cb_addr = 0;

  BCM2835_DMA_LOG("bcm2835_dma_program_cb #%d, %p", cb_handle, cb);
  asm volatile ("dsb sy");
  return true;
}

bool bcm2835_dma_update_cb_src(int cb_handle, uint32_t src)
{
  struct bcm2835_dma_cb *cb;

  if (cb_handle == -1 || cb_handle >= BCM2835_DMA_NUM_SCBS)
    return false;

  cb = &bcm2835_dma.cbs[cb_handle];
  cb->source_addr = src;
  return true;
}

int bcm2835_dma_request_channel(void)
{
  int ret = bitmap_set_next_free(&bcm2835_dma.channel_bitmap);
  BCM2835_DMA_LOG("bcm2835_dma_request_channel, ret:#%d", ret);
  return ret;
}

void bcm2835_dma_enable(int channel)
{
  BCM2835_DMA_LOG("bcm2835_dma_enable, ch:%d", channel);
  *DMA_ENABLE |= 1 << channel;
}

void bcm2835_dma_activate(int channel)
{
  BCM2835_DMA_LOG("bcm2835_dma_activate, ch:%d", channel);
  *DMA_CS(channel) |= DMA_CS_ACTIVE;
}

void bcm2835_dma_reset(int channel)
{
  BCM2835_DMA_LOG("bcm2835_dma_reset, ch:%d", channel);
  *DMA_CS(channel) |= DMA_CS_RESET;
}

void bcm2835_dma_poll(int channel)
{
  while (((*DMA_CS(channel)) & DMA_CS_END) == 0);
}

void bcm2835_dma_set_cb(int channel, int cb_handle)
{
  uint64_t cb_addr = (uint64_t)&bcm2835_dma.cbs[cb_handle];
  uint32_t paddr = RAM_PHY_TO_BUS_UNCACHED(cb_addr);

  BCM2835_DMA_LOG("bcm2835_dma_set_cb, ch:%d, cb#%d, va:%lx, pa:%08x",
    channel, cb_handle, cb_addr, paddr);

  *DMA_CONBLK_AD(channel) = paddr;
}

int bcm2835_dma_reserve_cb(void)
{
  int ret = bitmap_set_next_free(&bcm2835_dma.cb_bitmap);
  BCM2835_DMA_LOG("bcm2835_dma_reserve_cb, cb#%d", ret);
  return ret;
}

void bcm2835_dma_release_cb(int cb_idx)
{
  BCM2835_DMA_LOG("bcm2835_dma_release_cb, cb#%d", cb_idx);
  bitmap_clear_entry(&bcm2835_dma.cb_bitmap, cb_idx);
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
  uint64_t addr;
  size_t control_blocks_size;
  control_blocks_size = sizeof(struct bcm2835_dma_cb) * BCM2835_DMA_NUM_SCBS;

  bcm2835_dma.cb_area = dma_alloc(control_blocks_size, 1);
  if (!bcm2835_dma.cb_area)
    return;

  addr = ((uint64_t)bcm2835_dma.cb_area + 0xff - 1) & ~0xfful;
  printf("Allocatd %d DMA control blocks at %p/%p, cb size:%d\r\n",
    BCM2835_DMA_NUM_SCBS, addr, bcm2835_dma.cb_area, control_blocks_size);

  bcm2835_dma.cbs = (void *)addr;
  bcm2835_dma.cb_bitmap.data = bcm2835_dma_cb_bitmap;
  bcm2835_dma.cb_bitmap.num_entries = BCM2835_DMA_NUM_SCBS;
  bitmap_clear_all(&bcm2835_dma.cb_bitmap);

  bcm2835_dma.channel_bitmap.data = &bcm2835_dma.channel_bitmap_data;
  bcm2835_dma.channel_bitmap.num_entries = BCM2835_DMA_NUM_CHANNELS;
  bitmap_clear_all(&bcm2835_dma.channel_bitmap);

  for (size_t i = 0; i < BCM2835_DMA_NUM_CHANNELS; ++i) {
    if (*DMA_CS(i) & DMA_CS_DISDEBUG)
      bitmap_mark_busy(&bcm2835_dma.channel_bitmap, i);
  }

  irq_set(BCM2835_IRQNR_DMA_0, bcm2835_dma_irq_handler_ch_0);
  irq_set(BCM2835_IRQNR_DMA_1, bcm2835_dma_irq_handler_ch_1);
  irq_set(BCM2835_IRQNR_DMA_2, bcm2835_dma_irq_handler_ch_2);
  irq_set(BCM2835_IRQNR_DMA_3, bcm2835_dma_irq_handler_ch_3);
  irq_set(BCM2835_IRQNR_DMA_4, bcm2835_dma_irq_handler_ch_4);
  irq_set(BCM2835_IRQNR_DMA_5, bcm2835_dma_irq_handler_ch_5);
  irq_set(BCM2835_IRQNR_DMA_6, bcm2835_dma_irq_handler_ch_6);
  irq_set(BCM2835_IRQNR_DMA_7, bcm2835_dma_irq_handler_ch_7);
  irq_set(BCM2835_IRQNR_DMA_8, bcm2835_dma_irq_handler_ch_8);
  irq_set(BCM2835_IRQNR_DMA_9, bcm2835_dma_irq_handler_ch_9);
  irq_set(BCM2835_IRQNR_DMA_10, bcm2835_dma_irq_handler_ch_10);
  irq_set(BCM2835_IRQNR_DMA_11, bcm2835_dma_irq_handler_ch_11);
}

static inline int cb_paddr_to_cb_idx(uint32_t paddr)
{
  return (paddr - DMA_CB0_PADDR) / sizeof(struct bcm2835_dma_cb);
}

void bcm2835_dma_dump_channel(const char *tag, int channel)
{
  struct bcm2835_dma_cb *cb;
  uint32_t next_cb_addr = *DMA_NEXT_CONBLK(channel);
  uint32_t paddr = *DMA_CONBLK_AD(channel);

  BCM2835_DMA_LOG("dma [%s] cb#%d, cs:%08x,conblk:%08x,ti:%08x,len:%d,nxt:%08x",
    tag, cb_paddr_to_cb_idx(paddr), *DMA_CS(channel), paddr, *DMA_TI(channel),
    *DMA_TXFR_LEN(channel), next_cb_addr);

  paddr = next_cb_addr;

  while (paddr) {
    cb = PADDR_UNCACHED_TO_PTR(paddr);
    BCM2835_DMA_LOG("->cb#%d %p, ti:%08x,s:%08x->%08x,len:%d",
      cb_paddr_to_cb_idx(paddr),
      cb, cb->transfer_info, cb->source_addr, cb->dest_addr,
      cb->transfer_len, cb->next_cb_addr);
    paddr = cb->next_cb_addr;
  }
}

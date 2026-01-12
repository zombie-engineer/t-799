#include "bcm2835_emmc_priv.h"
#include <bcm2835_dma.h>
#include <errcode.h>

int bcm2835_emmc_io_init(struct bcm2835_emmc_io *io, void (*on_dma_done)(void))
{
  io->dma_channel = bcm2835_dma_request_channel();
  io->dma_control_block_idx = bcm2835_dma_reserve_cb();

  if (io->dma_channel == -1 || io->dma_control_block_idx == -1)
    return ERR_GENERIC;

  bcm2835_dma_set_irq_callback(io->dma_channel, on_dma_done);

  bcm2835_dma_irq_enable(io->dma_channel);
  bcm2835_dma_enable(io->dma_channel);
  bcm2835_dma_reset(io->dma_channel);

  return SUCCESS;
}


#pragma once
#include <stdbool.h>
#include <stdint.h>

struct bcm2835_emmc_cmd;

typedef enum {
  BCM2835_EMMC_IO_WAIT_CMD_DONE,
  BCM2835_EMMC_IO_WAIT_IO_READY,
  BCM2835_EMMC_IO_WAIT_DATA_DONE,
} bcm2835_emmc_cmd_state_t;

struct bcm2835_emmc_io {
  struct bcm2835_emmc_cmd *c;
  uint32_t cmdreg;
  int err;
  uint32_t interrupt;
  int dma_channel;
  int dma_control_block_idx;
};

struct bcm2835_emmc {
  bool is_blocking_mode;
  bool is_initialized;
  uint32_t device_id[4];
  uint32_t rca;
  struct bcm2835_emmc_io io;
  int num_inhibit_waits;
};

int bcm2835_emmc_io_init(struct bcm2835_emmc_io *io,
  void (*on_dma_done)(void));

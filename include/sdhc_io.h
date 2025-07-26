#pragma once
#include <stdbool.h>
#include <stdint.h>

struct sd_cmd;

typedef enum {
  SDHC_CMD_WAIT_CMD_DONE,
  SDHC_CMD_WAIT_IO_READY,
  SDHC_CMD_WAIT_DATA_DONE,
} sdhc_cmd_state_t;

struct sdhc_io {
  struct sd_cmd *c;
  uint32_t cmdreg;
  int err;
  uint32_t interrupt;
  int dma_channel;
  int dma_control_block_idx;

  /* Number of irqs that have been fired for this command */
  int num_irqs;
};

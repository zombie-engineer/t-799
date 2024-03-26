#pragma once
#include <stringlib.h>
#include <errcode.h>
#include <stdbool.h>

/* GO_IDLE */
#define BCM2835_EMMC_CMD0                 0x00000000
/* SEND_ALL_CID */
#define BCM2835_EMMC_CMD2                 0x00000002
/* SEND_RELATIVE_ADDRESS */
#define BCM2835_EMMC_CMD3                 0x00000003
/* CHECK SDIO */
#define BCM2835_EMMC_CMD5                 0x00000005
/* SELECT_CARD */
#define BCM2835_EMMC_CMD7                 0x00000007
/* SEND_IF_COND */
#define BCM2835_EMMC_CMD8                 0x00000008
#define BCM2835_EMMC_CMD8_ARG             0x000001aa
#define BCM2835_EMMC_CMD8_VALID_RESP      BCM2835_EMMC_CMD8_ARG
/* STOP_TRANSMISSION */
#define BCM2835_EMMC_CMD12                0x0000000c
/* SEND_STATUS */
#define BCM2835_EMMC_CMD13                0x0000000d
/* READ_SINGLE_BLOCK */
#define BCM2835_EMMC_CMD17                0x00000011
/* READ_MULTIPLE_BLOCK */
#define BCM2835_EMMC_CMD18                0x00000012
/* WRITE_BLOCK */
#define BCM2835_EMMC_CMD24                0x00000018
/* APP_CMD */
#define BCM2835_EMMC_CMD55                0x00000037

#define ACMD_BIT 31
#define ACMD(__idx) ((1<<ACMD_BIT) | __idx)
#define BCM2835_EMMC_CMD_IS_ACMD(__cmd) (__cmd & (1<<ACMD_BIT) ? 1 : 0)
#define BCM2835_EMMC_ACMD_RAW_IDX(__cmd) (__cmd & ~(1<<ACMD_BIT))

/* SET_BUS_WIDTH */
#define BCM2835_EMMC_BUS_WIDTH_1BIT 0
#define BCM2835_EMMC_BUS_WIDTH_4BITS 2
#define BCM2835_EMMC_ACMD6                0x80000006

#define BCM2835_EMMC_ACMD41               0x80000029
#define BCM2835_EMMC_ACMD51               0x80000033

struct bcm2835_emmc_cmd {
  uint32_t cmd_idx;
  uint32_t arg;
  uint32_t block_size;
  uint32_t num_blocks;
  uint32_t rca;
  uint32_t resp0;
  uint32_t resp1;
  uint32_t resp2;
  uint32_t resp3;
  int status;
  char *databuf;
};

static inline void bcm2835_emmc_cmd_init(struct bcm2835_emmc_cmd *c,
  int cmd_idx, int arg)
{
  memset(c, 0, sizeof(*c));
  c->cmd_idx = cmd_idx;
  c->arg = arg;
}

int bcm2835_emmc_cmd(struct bcm2835_emmc_cmd *c, uint64_t timeout_usec,
  bool blocking);

/*
 * Reset state after a failed command
 */
int bcm2835_emmc_reset_cmd(bool blocking);

/* Select card */
static inline int bcm2835_emmc_cmd7(uint32_t rca, bool blocking)
{
  struct bcm2835_emmc_cmd c;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD7, rca << 16);
  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

static inline int bcm2835_emmc_cmd13(uint32_t rca, uint32_t *out_status,
  bool blocking)
{
  struct bcm2835_emmc_cmd c;
  int cmd_ret;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD13, rca << 16);
  cmd_ret = bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);

  if (cmd_ret != SUCCESS)
    return cmd_ret;

  *out_status = c.resp0;

  return SUCCESS;
}

/* READ_SINGLE_BLOCK */
static inline int bcm2835_emmc_cmd17(uint32_t block_idx, char *dstbuf,
  bool blocking)
{
  struct bcm2835_emmc_cmd c;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD17, block_idx);
  c.databuf = dstbuf;
  c.num_blocks = 1;
  c.block_size = 512;

  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

/* WRITE_BLOCK */
static inline int bcm2835_emmc_cmd24(uint32_t block_idx, char *srcbuf,
  bool blocking)
{
  struct bcm2835_emmc_cmd c;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD24, block_idx);
  c.databuf = srcbuf;
  c.num_blocks = 1;
  c.block_size = 512;

  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

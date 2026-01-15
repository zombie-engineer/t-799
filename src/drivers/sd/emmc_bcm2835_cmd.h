#pragma once
#include <stringlib.h>
#include <errcode.h>
#include <stdbool.h>
#include "emmc_bcm2835_regs.h"
#include <printf.h>
#include <bitops.h>
#include <log.h>

/* GO_IDLE */
#define BCM2835_EMMC_CMD0                 0x00000000
/* SEND_ALL_CID */
#define BCM2835_EMMC_CMD2                 0x00000002
/* SEND_RELATIVE_ADDRESS */
#define BCM2835_EMMC_CMD3                 0x00000003
/* CHECK SDIO */
#define BCM2835_EMMC_CMD5                 0x00000005
/* CMD6 */
#define BCM2835_EMMC_CMD6                 0x00000006
/* SELECT_CARD */
#define BCM2835_EMMC_CMD7                 0x00000007
/* SEND_IF_COND */
#define BCM2835_EMMC_CMD8                 0x00000008
/* SEND_CSD */
#define BCM2835_EMMC_CMD9                 0x00000009
#define BCM2835_EMMC_CMD11                0x0000000b
/* STOP_TRANSMISSION */
#define BCM2835_EMMC_CMD12                0x0000000c
/* SEND_STATUS */
#define BCM2835_EMMC_CMD13                0x0000000d
/* READ_SINGLE_BLOCK */
#define BCM2835_EMMC_CMD17                0x00000011
/* READ_MULTIPLE_BLOCKS */
#define BCM2835_EMMC_CMD18                0x00000012
/* SET_BLOCK_COUNT */
#define BCM2835_EMMC_CMD23                0x00000017
/* WRITE_BLOCK */
#define BCM2835_EMMC_CMD24                0x00000018
/* WRITE_MULTIPLE_BLOCKS */
#define BCM2835_EMMC_CMD25                0x00000019
/* ERASE_WR_BLK_START */
#define BCM2835_EMMC_CMD32                0x00000020
/* ERASE_WR_BLK_END */
#define BCM2835_EMMC_CMD33                0x00000021
/* ERASE */
#define BCM2835_EMMC_CMD38                0x00000026
/* APP_CMD */
#define BCM2835_EMMC_CMD55                0x00000037
/* READ_OCR */
#define BCM2835_EMMC_CMD58                0x0000003a

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

struct sd_cmd {
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

#define SD_CMD_INIT(__idx, __arg, __rca) { \
  .cmd_idx = __idx, \
  .arg     = __arg, \
  .rca     = __rca, \
}

static inline void sd_cmd_init(struct sd_cmd *c,
  int cmd_idx, int arg)
{
  memset(c, 0, sizeof(*c));
  c->cmd_idx = cmd_idx;
  c->arg = arg;
}

int bcm2835_emmc_cmd(struct sd_cmd *c, uint64_t timeout_usec,
  bool blocking);

static inline int bcm2835_emmc_cmd0(bool blocking)
{
  struct sd_cmd c = SD_CMD_INIT(0, 0, 0);
  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

/* SEND_ALL_CID */
static inline int bcm2835_emmc_cmd2(uint32_t *device_id, bool blocking)
{
  int err;
  struct sd_cmd c = SD_CMD_INIT(2, 0, 0);
  err = bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);

  if (err != SUCCESS)
    return err;

  device_id[0] = c.resp0;
  device_id[1] = c.resp1;
  device_id[2] = c.resp2;
  device_id[3] = c.resp3;

  return SUCCESS;
}

static inline int bcm2835_emmc_cmd3(uint32_t *out_rca, bool blocking)
{
  uint32_t rca;
  bool __attribute__((unused)) crc_error;
  bool __attribute__((unused)) illegal_cmd;
  bool error;
  bool __attribute__((unused)) status;
  bool __attribute__((unused)) ready;

  int cmd_ret;
  struct sd_cmd c;

  sd_cmd_init(&c, BCM2835_EMMC_CMD3, 0);
  cmd_ret = bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);

  if (cmd_ret != SUCCESS)
    return cmd_ret;

  crc_error = BITS_EXTRACT32(c.resp0, 15, 1);
  illegal_cmd = BITS_EXTRACT32(c.resp0, 14, 1);
  error = BITS_EXTRACT32(c.resp0, 13, 1);
  status = BITS_EXTRACT32(c.resp0, 9, 1);
  ready = BITS_EXTRACT32(c.resp0, 8, 1);
  rca = BITS_EXTRACT32(c.resp0, 16, 16);

  if (error)
    return ERR_GENERIC;

  *out_rca = rca;

  return SUCCESS;
}

#define CMD6_MODE_CHECK 0
#define CMD6_MODE_SWITCH 1

#define CMD6_ARG_ACCESS_MODE_SDR12  0
#define CMD6_ARG_ACCESS_MODE_SDR25  1
#define CMD6_ARG_ACCESS_MODE_SDR50  2
#define CMD6_ARG_ACCESS_MODE_SDR104 3
#define CMD6_ARG_ACCESS_MODE_DDR50  4
#define CMD6_ARG_ACCESS_MODE_DEFAULT 0xf

#define CMD6_ARG_CMD_SYSTEM_EC      1
#define CMD6_ARG_CMD_SYSTEM_OTP     3
#define CMD6_ARG_CMD_SYSTEM_ASSD    4
#define CMD6_ARG_CMD_SYSTEM_DEFAULT 0xf

#define CMD6_ARG_DRIVER_STRENGTH_TYPE_B 0
#define CMD6_ARG_DRIVER_STRENGTH_TYPE_A 1
#define CMD6_ARG_DRIVER_STRENGTH_TYPE_C 2
#define CMD6_ARG_DRIVER_STRENGTH_TYPE_D 3
#define CMD6_ARG_DRIVER_STRENGTH_DEFAULT 0xf

#define CMD6_ARG_POWER_LIMIT_0_72W 0
#define CMD6_ARG_POWER_LIMIT_1_44W 1
#define CMD6_ARG_POWER_LIMIT_2_16W 2
#define CMD6_ARG_POWER_LIMIT_2_88W 3
#define CMD6_ARG_POWER_LIMIT_1_80W 4
#define CMD6_ARG_POWER_LIMIT_DEFAULT 0xf

static inline int bcm2835_emmc_cmd6(int mode, int access_mode,
  int command_system, int driver_strength, int power_limit, char *dstbuf,
  bool blocking)
{
  int err;
  struct sd_cmd c;
  uint32_t arg = access_mode & 0xf;

  arg |= (command_system & 0xf) << 4;
  arg |= (driver_strength & 0xf) << 8;
  arg |= (power_limit & 0xf) << 12;

  /*
   * Set reserved groups to default according to spec
   * Physical Layer Simplified Specification Version 9.10,
   * Chapter 4.3.10.2
   */
  arg |= (0xf << 16) | (0xf << 20);

  arg |= (mode & 1) << 31;
  // arg = 0x00fffff1;
  // printf("cmd6: arg:%08x\r\n", arg);

  sd_cmd_init(&c, BCM2835_EMMC_CMD6, arg);

  c.databuf = dstbuf;
  c.num_blocks = 1;
  c.block_size = 64;

  err = bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);

  if (err != SUCCESS)
    return err;

#if 0
  printf("cmd6: resp:%08x,%08x,%08x,%08x\r\n",
    c.resp0,
    c.resp1,
    c.resp2,
    c.resp3);
#endif
  return SUCCESS;
}


/* Select card */
static inline int bcm2835_emmc_cmd7(uint32_t rca, bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, BCM2835_EMMC_CMD7, rca << 16);
  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

static inline int bcm2835_emmc_cmd8(bool blocking)
{
  int ret;
  struct sd_cmd c;

  /*
   * CMD8 Send Interface condition command
   * Argument format:
   * [7-0]: Pattern that will be sent back by sd card in response, we will
   * check it then
   * [11-8]: Voltage supplied (VHS):
   *         0000 : Not defined
   *         0001 : 2.7-3.6V
   *         0010 : Reserved
   *         0100 : Reserved
   *         1000 : Reserved
   */
#define CMD8_ARG_PATTERN 0xff
#define CMD8_ARG_VOLTAGE_3V3 1
#define CMD8_ARG ((CMD8_ARG_VOLTAGE_3V3 << 8) | CMD8_ARG_PATTERN)
#define CMD8_EXPECTED_RESPONSE CMD8_ARG

  sd_cmd_init(&c, BCM2835_EMMC_CMD8, CMD8_ARG);
  ret = bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
  if (ret != SUCCESS)
    return ret;

  if (c.resp0 != CMD8_EXPECTED_RESPONSE) {
    printf("CMD8:unexpected response: %08x\r\n", c.resp0);
    return ERR_GENERIC;
  }

  return SUCCESS;
}

static inline int bcm2835_emmc_cmd9(uint32_t rca, bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, BCM2835_EMMC_CMD9, rca << 16);
  int err = bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
  printf("CSD:%08x %08x %08x %08x\r\n",
    ioreg32_read(BCM2835_EMMC_RESP0),
    ioreg32_read(BCM2835_EMMC_RESP1),
    ioreg32_read(BCM2835_EMMC_RESP2),
    ioreg32_read(BCM2835_EMMC_RESP3));
  return err;
}


static inline int bcm2835_emmc_cmd13(uint32_t rca, uint32_t *out_status,
  bool blocking)
{
  struct sd_cmd c;
  int cmd_ret;

  sd_cmd_init(&c, BCM2835_EMMC_CMD13, rca << 16);
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
  struct sd_cmd c;

  sd_cmd_init(&c, BCM2835_EMMC_CMD17, block_idx);
  c.databuf = dstbuf;
  c.num_blocks = 1;
  c.block_size = 512;

  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

/* READ_MULTIPLE_BLOCK */
static inline int bcm2835_emmc_cmd18(uint32_t block_idx, size_t num_blocks,
  char *dstbuf, bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, BCM2835_EMMC_CMD18, block_idx);
  c.databuf = dstbuf;
  c.num_blocks = num_blocks;
  c.block_size = 512;

  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

extern bool emmc_should_log;

static inline int bcm2835_emmc_cmd23(size_t num_blocks, bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, BCM2835_EMMC_CMD23, num_blocks);
  c.databuf = NULL;
  c.num_blocks = 0;
  c.block_size = 0;

  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

/* WRITE_BLOCK */
static inline int bcm2835_emmc_cmd24(uint32_t block_idx, char *srcbuf,
  bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, BCM2835_EMMC_CMD24, block_idx);
  c.databuf = srcbuf;
  c.num_blocks = 1;
  c.block_size = 512;

  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

int bcm2835_emmc_cmd25_nonstop(uint32_t block_idx);

static inline int bcm2835_emmc_cmd25(uint32_t block_idx, size_t num_blocks,
  char *srcbuf, bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, BCM2835_EMMC_CMD25, block_idx);
  c.databuf = srcbuf;
  c.num_blocks = num_blocks;
  c.block_size = 512;

  int err = bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
  printf("CMD25 done with err:%d\r\n", err);
  return err;
}

static inline int bcm2835_emmc_cmd32(uint32_t block_idx, bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, BCM2835_EMMC_CMD32, block_idx);
  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

static inline int bcm2835_emmc_cmd33(uint32_t block_idx, bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, BCM2835_EMMC_CMD33, block_idx);
  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

static inline int bcm2835_emmc_cmd38(uint32_t arg, bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, BCM2835_EMMC_CMD38, arg);
  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

static inline int bcm2835_emmc_cmd58(uint32_t rca, bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, BCM2835_EMMC_CMD58, rca << 16);
  int err = bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
  printf("OCR:%08x %08x %08x %08x\r\n", c.resp0, c.resp1, c.resp2, c.resp3);
  return err;
}

static inline int bcm2835_emmc_acmd6(uint32_t rca, uint32_t bus_width_bit,
  bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, BCM2835_EMMC_ACMD6, bus_width_bit);
  c.rca = rca;
  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

static inline int bcm2835_emmc_acmd41(uint32_t arg, uint32_t *resp,
  bool blocking)
{
  int err;
  struct sd_cmd c = SD_CMD_INIT(ACMD(41), arg, 0);
  err = bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);

#if 0
  BCM2835_EMMC_LOG("ACMD41 full response: %08x|%08x|%08x|%08x\r\n",
    c.resp0, c.resp1, c.resp2, c.resp3);
#endif

  if (err == SUCCESS)
    *resp = c.resp0;
  return err;
}

/* SEND_SCR */
static inline int bcm2835_emmc_acmd51(uint32_t rca, char *scrbuf,
  int scrbuf_len, bool blocking)
{
  struct sd_cmd c = SD_CMD_INIT(ACMD(51), 0, rca);

  if (scrbuf_len < 8)
    return -1;

  if ((uint64_t)scrbuf & 3)
    return -1;

  c.block_size = 8;
  c.num_blocks = 1;
  c.databuf = scrbuf;

  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC * 4, blocking);
}

void bcm2835_emmc_irq_handler(void);

void bcm2835_emmc_dma_irq_callback(void);

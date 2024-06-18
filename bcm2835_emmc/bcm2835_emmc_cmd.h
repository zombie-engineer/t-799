#pragma once
#include <stringlib.h>
#include <errcode.h>
#include <stdbool.h>
#include <printf.h>
#include <bitops.h>

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

static inline int bcm2835_emmc_cmd0(bool blocking)
{
  struct bcm2835_emmc_cmd c;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD0, 0);
  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

/* SEND_ALL_CID */
static inline int bcm2835_emmc_cmd2(uint32_t *device_id, bool blocking)
{
  int cmd_ret;
  struct bcm2835_emmc_cmd c;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD2, 0);
  cmd_ret = bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);

  if (cmd_ret != SUCCESS)
    return cmd_ret;

  device_id[0] = c.resp0;
  device_id[1] = c.resp1;
  device_id[2] = c.resp2;
  device_id[3] = c.resp3;

  return SUCCESS;
}

static inline int bcm2835_emmc_cmd3(uint32_t *out_rca, bool blocking)
{
  uint32_t rca;
  bool crc_error;
  bool illegal_cmd;
  bool error;
  bool status;
  bool ready;

  int cmd_ret;
  struct bcm2835_emmc_cmd c;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD3, 0);
  cmd_ret = bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);

  if (cmd_ret != SUCCESS)
    return cmd_ret;

  crc_error = BITS_EXTRACT32(c.resp0, 15, 1);
  illegal_cmd = BITS_EXTRACT32(c.resp0, 14, 1);
  error = BITS_EXTRACT32(c.resp0, 13, 1);
  status = BITS_EXTRACT32(c.resp0, 9, 1);
  ready = BITS_EXTRACT32(c.resp0, 8, 1);
  rca = BITS_EXTRACT32(c.resp0, 16, 16);

#if 0
  BCM2835_EMMC_LOG("bcm2835_emmc_cmd3 result: err: %d, crc_err: %d,"
    " illegal_cmd: %d, status: %d, ready: %d, rca: %04x",
    error, crc_error, illegal_cmd, status, ready, rca);
#endif

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
#define CMD6_ARG_ACCESS_MODE_DEFAULT CMD6_ARG_ACCESS_MODE_SDR12

#define CMD6_ARG_CMD_SYSTEM_DEFAULT 0
#define CMD6_ARG_CMD_SYSTEM_EC      1
#define CMD6_ARG_CMD_SYSTEM_OTP     3
#define CMD6_ARG_CMD_SYSTEM_ASSD    4

#define CMD6_ARG_DRIVER_STRENGTH_TYPE_B 0
#define CMD6_ARG_DRIVER_STRENGTH_TYPE_A 1
#define CMD6_ARG_DRIVER_STRENGTH_TYPE_C 2
#define CMD6_ARG_DRIVER_STRENGTH_TYPE_D 3
#define CMD6_ARG_DRIVER_STRENGTH_DEFAULT CMD6_ARG_DRIVER_STRENGTH_TYPE_B

#define CMD6_ARG_POWER_LIMIT_0_72W 0
#define CMD6_ARG_POWER_LIMIT_1_44W 1
#define CMD6_ARG_POWER_LIMIT_2_16W 2
#define CMD6_ARG_POWER_LIMIT_2_88W 3
#define CMD6_ARG_POWER_LIMIT_1_80W 4
#define CMD6_ARG_POWER_LIMIT_DEFAULT CMD6_ARG_POWER_LIMIT_0_72W

static inline int bcm2835_emmc_cmd6(int mode, int access_mode,
  int command_system, int driver_strength, int power_limit, char *dstbuf,
  bool blocking)
{
  int err;
  struct bcm2835_emmc_cmd c;
  uint32_t arg = access_mode & 0xf;

  arg |= (command_system & 0xf) << 4;
  arg |= (driver_strength & 0xf) << 8;
  arg |= (power_limit & 0xf) << 12;
  arg |= (mode & 1) << 31;
  arg |= 0xff;
  printf("cmd6: arg:%08x\r\n", arg);
  arg = 0xfffffff0;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD6, arg);

  c.databuf = dstbuf;
  c.num_blocks = 1;
  c.block_size = 512;

  err = bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
  printf("CMD6: ret %d\r\n", err);

  if (err != SUCCESS)
    return err;

  printf("cmd6: resp:%08x,%08x,%08x,%08x\r\n",
    c.resp0,
    c.resp1,
    c.resp2,
    c.resp3);
  return SUCCESS;
}


/* Select card */
static inline int bcm2835_emmc_cmd7(uint32_t rca, bool blocking)
{
  struct bcm2835_emmc_cmd c;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD7, rca << 16);
  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

static inline int bcm2835_emmc_cmd8(bool blocking)
{
  int ret;
  struct bcm2835_emmc_cmd c;

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

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD8, CMD8_ARG);
  ret = bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
  if (ret != SUCCESS)
    return ret;

  if (c.resp0 != CMD8_EXPECTED_RESPONSE)
    return ERR_GENERIC;

  return SUCCESS;
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

/* READ_MULTIPLE_BLOCK */
static inline int bcm2835_emmc_cmd18(uint32_t block_idx, size_t num_blocks,
  char *dstbuf, bool blocking)
{
  struct bcm2835_emmc_cmd c;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD18, block_idx);
  c.databuf = dstbuf;
  c.num_blocks = num_blocks;
  c.block_size = 512;

  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

extern bool emmc_should_log;

static inline int bcm2835_emmc_cmd23(size_t num_blocks, bool blocking)
{
  struct bcm2835_emmc_cmd c;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD23, num_blocks);
  c.databuf = NULL;
  c.num_blocks = 0;
  c.block_size = 0;

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

int bcm2835_emmc_cmd25_nonstop(uint32_t block_idx);

static inline int bcm2835_emmc_cmd25(uint32_t block_idx, size_t num_blocks,
  char *srcbuf, bool blocking)
{
  struct bcm2835_emmc_cmd c;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD25, block_idx);
  c.databuf = srcbuf;
  c.num_blocks = num_blocks;
  c.block_size = 512;

  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

static inline int bcm2835_emmc_cmd32(uint32_t block_idx, bool blocking)
{
  struct bcm2835_emmc_cmd c;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD32, block_idx);
  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

static inline int bcm2835_emmc_cmd33(uint32_t block_idx, bool blocking)
{
  struct bcm2835_emmc_cmd c;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD33, block_idx);
  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

static inline int bcm2835_emmc_cmd38(uint32_t arg, bool blocking)
{
  struct bcm2835_emmc_cmd c;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD38, arg);
  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

void bcm2835_emmc_irq_handler(void);

void bcm2835_emmc_dma_irq_callback(void);

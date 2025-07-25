#pragma once
#include <string.h>
#include <printf.h>

/* GO_IDLE */
#define SDHC_CMD0                 0x00000000
/* SEND_ALL_CID */
#define SDHC_CMD2                 0x00000002
/* SEND_RELATIVE_ADDRESS */
#define SDHC_CMD3                 0x00000003
/* CHECK SDIO */
#define SDHC_CMD5                 0x00000005
/* CMD6 */
#define SDHC_CMD6                 0x00000006
/* SELECT_CARD */
#define SDHC_CMD7                 0x00000007
/* SEND_IF_COND */
#define SDHC_CMD8                 0x00000008
/* SEND_CSD */
#define SDHC_CMD9                 0x00000009

#define SDHC_CMD11                0x0000000b
/* STOP_TRANSMISSION */
#define SDHC_CMD12                0x0000000c
/* SEND_STATUS */
#define SDHC_CMD13                0x0000000d
/* READ_SINGLE_BLOCK */
#define SDHC_CMD17                0x00000011
/* READ_MULTIPLE_BLOCKS */
#define SDHC_CMD18                0x00000012
/* SET_BLOCK_COUNT */
#define SDHC_CMD23                0x00000017
/* WRITE_BLOCK */
#define SDHC_CMD24                0x00000018
/* WRITE_MULTIPLE_BLOCKS */
#define SDHC_CMD25                0x00000019
/* ERASE_WR_BLK_START */
#define SDHC_CMD32                0x00000020
/* ERASE_WR_BLK_END */
#define SDHC_CMD33                0x00000021
/* ERASE */
#define SDHC_CMD38                0x00000026
/* APP_CMD */
#define SDHC_CMD55                0x00000037
/* READ_OCR */
#define SDHC_CMD58                0x0000003a

#define ACMD_BIT 31
#define ACMD(__idx) ((1<<ACMD_BIT) | __idx)
#define SDHC_CMD_IS_ACMD(__cmd) (__cmd & (1<<ACMD_BIT) ? 1 : 0)
#define SDHC_ACMD_RAW_IDX(__cmd) (__cmd & ~(1<<ACMD_BIT))

/* SET_BUS_WIDTH */
#define SDHC_ACMD6_ARG_BUS_WIDTH_1 0
#define SDHC_ACMD6_ARG_BUS_WIDTH_4 2
#define SDHC_ACMD6 ACMD(6)

#define SDHC_ACMD41 ACMD(41)
#define SDHC_ACMD51 ACMD(51)

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

int sdhc_cmd(struct sd_cmd *c, uint64_t timeout_usec,
  bool blocking);

static inline int sdhc_cmd0(uint64_t timeout_usec, bool blocking)
{
  struct sd_cmd c = SD_CMD_INIT(0, 0, 0);
  return sdhc_cmd(&c, timeout_usec, blocking);
}

/* SEND_ALL_CID */
static inline int sdhc_cmd2(uint32_t *device_id, uint64_t timeout_usec,
  bool blocking)
{
  int err;
  struct sd_cmd c = SD_CMD_INIT(2, 0, 0);
  err = sdhc_cmd(&c, timeout_usec, blocking);

  if (err != SUCCESS)
    return err;

  device_id[0] = c.resp0;
  device_id[1] = c.resp1;
  device_id[2] = c.resp2;
  device_id[3] = c.resp3;

  return SUCCESS;
}

static inline int sdhc_cmd3(uint32_t *out_rca, uint64_t timeout_usec,
  bool blocking)
{
  uint32_t rca;
  bool crc_error;
  bool illegal_cmd;
  bool error;
  bool status;
  bool ready;

  int cmd_ret;
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD3, 0);
  cmd_ret = sdhc_cmd(&c, timeout_usec, blocking);

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

static inline int sdhc_cmd6(int mode, int access_mode,
  int command_system, int driver_strength, int power_limit, uint8_t *dstbuf,
  uint64_t timeout_usec, bool blocking)
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

  sd_cmd_init(&c, SDHC_CMD6, arg);

  c.databuf = dstbuf;
  c.num_blocks = 1;
  c.block_size = 64;

  return sdhc_cmd(&c, timeout_usec, blocking);
}


/* Select card */
static inline int sdhc_cmd7(uint32_t rca, uint64_t timeout_usec, bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD7, rca << 16);
  return sdhc_cmd(&c, timeout_usec, blocking);
}

static inline int sdhc_cmd8(uint64_t timeout_usec, bool blocking)
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
#define CMD8_ARG_PATTERN 0xdd
#define CMD8_ARG_VOLTAGE_3V3 1
#define CMD8_ARG ((CMD8_ARG_VOLTAGE_3V3 << 8) | CMD8_ARG_PATTERN)
#define CMD8_EXPECTED_RESPONSE CMD8_ARG

  sd_cmd_init(&c, SDHC_CMD8, CMD8_ARG);
  ret = sdhc_cmd(&c, timeout_usec, blocking);
  if (ret != SUCCESS)
    return ret;

  if (c.resp0 != CMD8_EXPECTED_RESPONSE) {
    printf("CMD8:unexpected response: %08x\r\n", c.resp0);
    return ERR_GENERIC;
  }

  return SUCCESS;
}

static inline int sdhc_cmd9(uint32_t rca, uint32_t *dst,
  uint64_t timeout_usec, bool blocking)
{
  int ret;

  struct sd_cmd c;
  sd_cmd_init(&c, SDHC_CMD9, rca << 16);
  ret = sdhc_cmd(&c, timeout_usec, blocking);
  if (ret == SUCCESS) {
    dst[0] = c.resp0;
    dst[1] = c.resp1;
    dst[2] = c.resp2;
    dst[3] = c.resp3;
    printf("%08x\r\n", dst[0]);
  }
  return ret;
}

static inline int sdhc_cmd13(uint32_t rca, uint32_t *out_status,
  uint64_t timeout_usec, bool blocking)
{
  struct sd_cmd c;
  int cmd_ret;

  sd_cmd_init(&c, SDHC_CMD13, rca << 16);
  cmd_ret = sdhc_cmd(&c, timeout_usec, blocking);

  if (cmd_ret != SUCCESS)
    return cmd_ret;

  *out_status = c.resp0;

  return SUCCESS;
}

/* READ_SINGLE_BLOCK */
static inline int sdhc_cmd17(uint32_t block_idx, char *dstbuf,
  uint64_t timeout_usec, bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD17, block_idx);
  c.databuf = dstbuf;
  c.num_blocks = 1;
  c.block_size = 512;

  return sdhc_cmd(&c, timeout_usec, blocking);
}

/* READ_MULTIPLE_BLOCK */
static inline int sdhc_cmd18(uint32_t block_idx, size_t num_blocks,
  char *dstbuf, uint64_t timeout_usec, bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD18, block_idx);
  c.databuf = dstbuf;
  c.num_blocks = num_blocks;
  c.block_size = 512;

  return sdhc_cmd(&c, timeout_usec, blocking);
}

extern bool emmc_should_log;

static inline int sdhc_cmd23(size_t num_blocks, uint64_t timeout_usec,
  bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD23, num_blocks);
  c.databuf = NULL;
  c.num_blocks = 0;
  c.block_size = 0;

  return sdhc_cmd(&c, timeout_usec, blocking);
}

/* WRITE_BLOCK */
static inline int sdhc_cmd24(uint32_t block_idx, char *srcbuf,
  uint64_t timeout_usec, bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD24, block_idx);
  c.databuf = srcbuf;
  c.num_blocks = 1;
  c.block_size = 512;

  return sdhc_cmd(&c, timeout_usec, blocking);
}

int sdhc_cmd25_nonstop(uint32_t block_idx);

static inline int sdhc_cmd25(uint32_t block_idx, size_t num_blocks,
  char *srcbuf, uint64_t timeout_usec, bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD25, block_idx);
  c.databuf = srcbuf;
  c.num_blocks = num_blocks;
  c.block_size = 512;

  int err = sdhc_cmd(&c, timeout_usec, blocking);
  printf("CMD25 done with err:%d\r\n", err);
  return err;
}

static inline int sdhc_cmd32(uint32_t block_idx, uint64_t timeout_usec,
  bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD32, block_idx);
  return sdhc_cmd(&c, timeout_usec, blocking);
}

static inline int sdhc_cmd33(uint32_t block_idx, uint64_t timeout_usec,
  bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD33, block_idx);
  return sdhc_cmd(&c, timeout_usec, blocking);
}

static inline int sdhc_cmd38(uint32_t arg, uint64_t timeout_usec,
  bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD38, arg);
  return sdhc_cmd(&c, timeout_usec, blocking);
}

static inline int sdhc_cmd58(uint32_t rca, uint64_t timeout_usec,
  bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD58, rca << 16);
  int err = sdhc_cmd(&c, timeout_usec, blocking);
  printf("OCR:%08x %08x %08x %08x\r\n", c.resp0, c.resp1, c.resp2, c.resp3);
  return err;
}

static inline int sdhc_acmd6(uint32_t rca, uint32_t bus_width_bit,
  uint64_t timeout_usec, bool blocking)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_ACMD6, bus_width_bit);
  c.rca = rca;
  return sdhc_cmd(&c, timeout_usec, blocking);
}

static inline int sdhc_acmd41(uint32_t arg, uint32_t rca, uint32_t *resp,
  uint64_t timeout_usec, bool blocking)
{
  int err;
  struct sd_cmd c = SD_CMD_INIT(ACMD(41), arg, rca);
  err = sdhc_cmd(&c, timeout_usec, blocking);

  if (err == SUCCESS)
    *resp = c.resp0;
  return err;
}

/* SEND_SCR */
static inline int sdhc_acmd51(uint32_t rca, void *scrbuf,
  int scrbuf_len, uint64_t timeout_usec, bool blocking)
{
  int ret;

  struct sd_cmd c = SD_CMD_INIT(ACMD(51), 0, rca);

  if (scrbuf_len < 8)
    return -1;

  if ((uint64_t)scrbuf & 3)
    return -1;

  c.block_size = 8;
  c.num_blocks = 1;
  c.databuf = scrbuf;

  return sdhc_cmd(&c, timeout_usec, blocking);
}

static inline int sdhc_acmd51x4(uint32_t rca, void *scrbuf,
  int scrbuf_len, uint64_t timeout_usec, bool blocking)
{
  int ret;

  struct sd_cmd c = SD_CMD_INIT(ACMD(51), 0, rca);

  if (scrbuf_len < 8)
    return -1;

  if ((uint64_t)scrbuf & 3)
    return -1;

  c.block_size = 1;
  c.num_blocks = 8;
  c.databuf = scrbuf;

  return sdhc_cmd(&c, timeout_usec, blocking);
}

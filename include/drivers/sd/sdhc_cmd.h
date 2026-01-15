#pragma once
#include <string.h>
#include <printf.h>
#include <common.h>
#include <bitops.h>
#include <drivers/sd/sdhc.h>

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
/* SET_BLOCK_LEN */
#define SDHC_CMD16                0x00000010
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
#define SDHC_ACMD13 ACMD(13)

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

static inline int sdhc_cmd0(struct sdhc *s, uint64_t timeout_usec)
{
  struct sd_cmd c = SD_CMD_INIT(0, 0, 0);
  return s->ops->cmd(s, &c, timeout_usec);
}

/* SEND_ALL_CID */
static inline int sdhc_cmd2(struct sdhc *s, uint32_t *device_id,
  uint64_t timeout_usec)
{
  int err;
  struct sd_cmd c = SD_CMD_INIT(2, 0, 0);
  err = s->ops->cmd(s, &c, timeout_usec);

  if (err != SUCCESS)
    return err;

  device_id[0] = c.resp0;
  device_id[1] = c.resp1;
  device_id[2] = c.resp2;
  device_id[3] = c.resp3;

  return SUCCESS;
}

static inline int sdhc_cmd3(struct sdhc *s, uint32_t *out_rca,
  uint64_t timeout_usec)
{
  int ret;
  uint32_t rca;
  bool __attribute__((unused)) crc_error;
  bool __attribute__((unused)) illegal_cmd;
  bool error;
  bool __attribute__((unused)) status;
  bool __attribute__((unused)) ready;

  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD3, 0);
  ret = s->ops->cmd(s, &c, timeout_usec);
  if (ret != SUCCESS)
    return ret;

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

static inline int sdhc_cmd6(struct sdhc *s, int mode, int access_mode,
  int command_system, int driver_strength, int power_limit, uint8_t *dstbuf,
  uint64_t timeout_usec)
{
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

  c.databuf = (void *)dstbuf;
  c.num_blocks = 1;
  c.block_size = 64;

  return s->ops->cmd(s, &c, timeout_usec);
}


/* Select card */
static inline int sdhc_cmd7(struct sdhc *s, uint64_t timeout_usec)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD7, s->rca << 16);
  return s->ops->cmd(s, &c, timeout_usec);
}

static inline int sdhc_cmd8(struct sdhc *s, uint64_t timeout_usec)
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
#define CMD8_ARG_PATTERN 0xaa
#define CMD8_ARG_VOLTAGE_3V3 1
#define CMD8_ARG ((CMD8_ARG_VOLTAGE_3V3 << 8) | CMD8_ARG_PATTERN)
#define CMD8_EXPECTED_RESPONSE CMD8_ARG

  sd_cmd_init(&c, SDHC_CMD8, CMD8_ARG);
  ret = s->ops->cmd(s, &c, timeout_usec);
  if (ret != SUCCESS)
    return ret;

  if (c.resp0 != CMD8_EXPECTED_RESPONSE) {
    printf("CMD8:unexpected response: %08x\r\n", c.resp0);
    return ERR_GENERIC;
  }

  return SUCCESS;
}

static inline int sdhc_cmd9(struct sdhc *s, uint32_t *dst,
  uint64_t timeout_usec)
{
  int ret;

  struct sd_cmd c;
  sd_cmd_init(&c, SDHC_CMD9, s->rca << 16);
  ret = s->ops->cmd(s, &c, timeout_usec);
  if (ret == SUCCESS) {
    dst[0] = c.resp0;
    dst[1] = c.resp1;
    dst[2] = c.resp2;
    dst[3] = c.resp3;
  }
  return ret;
}

static inline int sdhc_cmd12(struct sdhc *s, uint64_t timeout_usec)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD12, 0);
  return s->ops->cmd(s, &c, timeout_usec);
}

static inline int sdhc_cmd13(struct sdhc *s, uint32_t *out_status,
  uint64_t timeout_usec)
{
  struct sd_cmd c;
  int cmd_ret;

  sd_cmd_init(&c, SDHC_CMD13, s->rca << 16);
  cmd_ret = s->ops->cmd(s, &c, timeout_usec);

  if (cmd_ret != SUCCESS)
    return cmd_ret;

  *out_status = c.resp0;

  return SUCCESS;
}

/* SET_BLOCK_LEN */
static inline int sdhc_cmd16(struct sdhc *s, uint32_t block_len,
  uint64_t timeout_usec)
{
  struct sd_cmd c;
  sd_cmd_init(&c, SDHC_CMD16, block_len);
  return s->ops->cmd(s, &c, timeout_usec);
}

/* READ_SINGLE_BLOCK */
static inline int sdhc_cmd17(struct sdhc *s, uint32_t block_idx, uint8_t *dstbuf,
  uint64_t timeout_usec)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD17, block_idx);
  c.databuf = (void *)dstbuf;
  c.num_blocks = 1;
  c.block_size = 512;

  return s->ops->cmd(s, &c, timeout_usec);
}

/* READ_MULTIPLE_BLOCK */
static inline int sdhc_cmd18(struct sdhc *s, uint32_t start_block_idx,
  size_t num_blocks, uint8_t *dstbuf, uint64_t timeout_usec)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD18, start_block_idx);
  c.databuf = (void *)dstbuf;
  c.num_blocks = num_blocks;
  c.block_size = 512;

  return s->ops->cmd(s, &c, timeout_usec);
}

static inline int sdhc_cmd23(struct sdhc *s, size_t num_blocks,
  uint64_t timeout_usec)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD23, num_blocks);
  c.databuf = NULL;
  c.num_blocks = 0;
  c.block_size = 0;

  return s->ops->cmd(s, &c, timeout_usec);
}

/* WRITE_BLOCK */
static inline int sdhc_cmd24(struct sdhc *s, uint32_t block_idx,
  const uint8_t *srcbuf, uint64_t timeout_usec)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD24, block_idx);
  c.databuf = (void *)srcbuf;
  c.num_blocks = 1;
  c.block_size = 512;

  return s->ops->cmd(s, &c, timeout_usec);
}

static inline int sdhc_cmd25(struct sdhc *s, uint32_t block_idx,
  size_t num_blocks, uint8_t *srcbuf, uint64_t timeout_usec)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD25, block_idx);
  c.databuf = (char *)srcbuf;
  c.num_blocks = num_blocks;
  c.block_size = 512;

  return s->ops->cmd(s, &c, timeout_usec);
}

static inline int sdhc_cmd32(struct sdhc *s, uint32_t block_idx,
  uint64_t timeout_usec)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD32, block_idx);
  return s->ops->cmd(s, &c, timeout_usec);
}

static inline int sdhc_cmd33(struct sdhc *s, uint32_t block_idx,
  uint64_t timeout_usec)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD33, block_idx);
  return s->ops->cmd(s, &c, timeout_usec);
}

static inline int sdhc_cmd38(struct sdhc *s, uint32_t arg,
  uint64_t timeout_usec)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD38, arg);
  return s->ops->cmd(s, &c, timeout_usec);
}

static inline int sdhc_cmd58(struct sdhc *s, uint64_t timeout_usec)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_CMD58, s->rca << 16);
  int ret = s->ops->cmd(s, &c, timeout_usec);
  printf("OCR:%08x %08x %08x %08x\r\n", c.resp0, c.resp1, c.resp2, c.resp3);
  return ret;
}

static inline int sdhc_acmd6(struct sdhc *s, uint32_t bus_width_bit,
  uint64_t timeout_usec)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_ACMD6, bus_width_bit);
  c.rca = s->rca;
  return s->ops->cmd(s, &c, timeout_usec);
}

static inline int sdhc_acmd13(struct sdhc *s, uint8_t *sd_status,
  uint64_t timeout_usec)
{
  struct sd_cmd c;

  sd_cmd_init(&c, SDHC_ACMD13, 0);
  c.rca = s->rca;

  c.block_size = 64;
  c.num_blocks = 1;
  c.databuf = (void *)sd_status;
  return s->ops->cmd(s, &c, timeout_usec);
}

/*
 * SD_SEND_OP_COND
 * Sends Host Capacity Support (HCS) info to SD card and requests from SD card
 * to send back Operation Conditions Register.
 * arg - HCS
 * resp = OCR
 */
static inline int sdhc_acmd41(struct sdhc *s, uint32_t arg, uint32_t *ocr,
  uint64_t timeout_usec)
{
  int ret;

  struct sd_cmd c = SD_CMD_INIT(ACMD(41), arg, s->rca);
  ret = s->ops->cmd(s, &c, timeout_usec);

  if (ret == SUCCESS)
    *ocr = c.resp0;
  return ret;
}

/* SEND_SCR */
static inline int sdhc_acmd51(struct sdhc *s, void *dst,
  int dst_size, uint64_t timeout_usec)
{
  int ret;
  uint64_t scr;

  struct sd_cmd c = SD_CMD_INIT(ACMD(51), 0, s->rca);

  if (dst_size < sizeof(uint64_t))
    return -1;

  if ((uint64_t)dst & 3)
    return -1;

  c.block_size = 8;
  c.num_blocks = 1;
  c.databuf = (void *)&scr;

  ret = s->ops->cmd(s, &c, timeout_usec);
  if (ret == SUCCESS)
    *(uint64_t *)dst = reverse_bytes64(scr);

  return ret;
}

#include <bitops.h>
#include <stringlib.h>
#include <bcm2835/bcm2835_emmc.h>
#include "bcm2835_emmc_cmd.h"
#include "bcm2835_emmc_utils.h"
#include "bcm2835_emmc_regs_bits.h"
#include "bcm2835_emmc_regs.h"

/* P1 Physical Layer Simplified Specification.  * 4.9 Responces */

/* 4.9.1 R1 (normal response command) */
#define RESP_TYPE_R1  BCM2835_EMMC_RESPONSE_TYPE_48_BITS

/* 4.9.2 R1b */
#define RESP_TYPE_R1b  BCM2835_EMMC_RESPONSE_TYPE_48_BITS_BUSY

/* 4.9.3 R2 (CID, CSD register) */
#define RESP_TYPE_R2  BCM2835_EMMC_RESPONSE_TYPE_136_BITS

/* 4.9.4 R3 (OCR register) */
#define RESP_TYPE_R3  BCM2835_EMMC_RESPONSE_TYPE_48_BITS

/* 4.9.5 R6 (Published RCA response) */
#define RESP_TYPE_R6  BCM2835_EMMC_RESPONSE_TYPE_48_BITS

/* 4.9.5 R7 (Card interface condition) */
#define RESP_TYPE_R7  BCM2835_EMMC_RESPONSE_TYPE_48_BITS

/* Stub response type */
#define RESP_TYPE_NA  BCM2835_EMMC_RESPONSE_TYPE_NONE

/* Transfer type host to card */
#define BCM2835_EMMC_TRANS_TYPE_DATA_HOST_TO_CARD 0

/* Transfer type stub */
#define BCM2835_EMMC_TRANS_TYPE_DATA_NA 0

/* Transfer type card to host */
#define BCM2835_EMMC_TRANS_TYPE_DATA_CARD_TO_HOST 1

#define CMDTM_GEN(__idx, __resp_type, __crc_enable, __trans_type, __is_data)\
  ((__idx << BCM2835_EMMC_CMDTM_SHIFT_CMD_INDEX) |\
  (RESP_TYPE_ ## __resp_type << BCM2835_EMMC_CMDTM_SHIFT_CMD_RSPNS_TYPE) |\
  (__crc_enable << BCM2835_EMMC_CMDTM_SHIFT_CMD_CRCCHK_EN) |\
  (BCM2835_EMMC_TRANS_TYPE_DATA_ ## __trans_type << BCM2835_EMMC_CMDTM_SHIFT_TM_DAT_DIR) |\
  (__is_data << BCM2835_EMMC_CMDTM_SHIFT_CMD_ISDATA))


static uint32_t sd_commands[] = {
  CMDTM_GEN(0,  NA,      0, NA, 0),
  CMDTM_GEN(1,  NA,      0, NA, 0),
  CMDTM_GEN(2,  R2,      1, NA, 0),
  CMDTM_GEN(3,  R6,      1, NA, 0),
  CMDTM_GEN(4,  NA,      0, NA, 0),
  CMDTM_GEN(5,  R1b,     0, NA, 0),
  CMDTM_GEN(6,  NA,      0, NA, 0),
  CMDTM_GEN(7,  R1b,     1, NA, 0),
  CMDTM_GEN(8,  R7,      1, NA, 0),
  CMDTM_GEN(9,  R2,      0, NA, 0),
  CMDTM_GEN(10, R2,      0, NA, 0),
  CMDTM_GEN(11, R1,      1, NA, 0),
  CMDTM_GEN(12, R1b,     1, NA, 0),
  CMDTM_GEN(13, R1,      1, NA, 0),
  CMDTM_GEN(14, NA,      0, NA, 0),
  CMDTM_GEN(15, NA,      0, NA, 0),
  CMDTM_GEN(16, R1,      1, NA, 0),
  CMDTM_GEN(17, R1,      1, CARD_TO_HOST, 1),
  CMDTM_GEN(18, R1,      1, NA, 0),
  CMDTM_GEN(19, R1,      1, NA, 0),
  CMDTM_GEN(20, R1b,     1, NA, 0),
  CMDTM_GEN(21, R1,      1, NA, 0),
  CMDTM_GEN(22, R1,      1, NA, 0),
  CMDTM_GEN(23, R1,      1, NA, 0),
  CMDTM_GEN(24, R1,      1, HOST_TO_CARD, 1),
  CMDTM_GEN(25, R1,      1, NA, 0),
  CMDTM_GEN(26, NA,      0, NA, 0),
  CMDTM_GEN(27, R1,      1, NA, 0),
  CMDTM_GEN(28, R1b,     1, NA, 0),
  CMDTM_GEN(29, R1b,     1, NA, 0),
  CMDTM_GEN(30, R1,      1, NA, 0),
  CMDTM_GEN(31, NA,      0, NA, 0),
  CMDTM_GEN(32, R1,      1, NA, 0),
  CMDTM_GEN(33, R1,      1, NA, 0),
  CMDTM_GEN(34, NA,      0, NA, 0),
  CMDTM_GEN(35, NA,      0, NA, 0),
  CMDTM_GEN(36, NA,      0, NA, 0),
  CMDTM_GEN(37, NA,      0, NA, 0),
  CMDTM_GEN(38, R1b,     1, NA, 0),
  CMDTM_GEN(39, NA,      0, NA, 0),
  CMDTM_GEN(40, R1,      1, NA, 0),
  CMDTM_GEN(41, NA,      0, NA, 0),
  CMDTM_GEN(42, R1,      1, NA, 0),
  CMDTM_GEN(43, NA,    0, NA, 0),
  CMDTM_GEN(44, NA,    0, NA, 0),
  CMDTM_GEN(45, NA,    0, NA, 0),
  CMDTM_GEN(46, NA,    0, NA, 0),
  CMDTM_GEN(47, NA,    0, NA, 0),
  CMDTM_GEN(48, NA,    0, NA, 0),
  CMDTM_GEN(49, NA,    0, NA, 0),
  CMDTM_GEN(50, NA,    0, NA, 0),
  CMDTM_GEN(51, NA,    0, NA, 0),
  CMDTM_GEN(52, NA,    0, NA, 0),
  CMDTM_GEN(53, NA,    0, NA, 0),
  CMDTM_GEN(54, NA,    0, NA, 0),
  CMDTM_GEN(55, R1,      1, NA, 0),
  CMDTM_GEN(56, R1,      1, NA, 0),
  CMDTM_GEN(57, NA,    0, NA, 0),
  CMDTM_GEN(58, NA,    0, NA, 0),
  CMDTM_GEN(59, NA,    0, NA, 0)
};

static uint32_t sd_acommands[] = {
  CMDTM_GEN(0, NA,    0, NA, 0),
  CMDTM_GEN(1, NA,    0, NA, 0),
  CMDTM_GEN(2, NA,    0, NA, 0),
  CMDTM_GEN(3, NA,    0, NA, 0),
  CMDTM_GEN(4, NA,    0, NA, 0),
  CMDTM_GEN(5, NA,    0, NA, 0),
  CMDTM_GEN(6, R1,    1, NA, 0),
  CMDTM_GEN(7, NA,    0, NA, 0),
  CMDTM_GEN(8, NA,    0, NA, 0),
  CMDTM_GEN(9, NA,    0, NA, 0),
  CMDTM_GEN(10, NA,    0, NA, 0),
  CMDTM_GEN(11, NA,    0, NA, 0),
  CMDTM_GEN(12, NA,    0, NA, 0),
  CMDTM_GEN(13, NA,    0, NA, 0),
  CMDTM_GEN(14, NA,    0, NA, 0),
  CMDTM_GEN(15, NA,    0, NA, 0),
  CMDTM_GEN(16, NA,    0, NA, 0),
  CMDTM_GEN(17, NA,    0, NA, 0),
  CMDTM_GEN(18, NA,    0, NA, 0),
  CMDTM_GEN(19, NA,    0, NA, 0),
  CMDTM_GEN(20, NA,    0, NA, 0),
  CMDTM_GEN(21, NA,    0, NA, 0),
  CMDTM_GEN(22, NA,    0, NA, 0),
  CMDTM_GEN(23, NA,    0, NA, 0),
  CMDTM_GEN(24, NA,    0, NA, 0),
  CMDTM_GEN(25, NA,    0, NA, 0),
  CMDTM_GEN(26, NA,    0, NA, 0),
  CMDTM_GEN(27, NA,    0, NA, 0),
  CMDTM_GEN(28, NA,    0, NA, 0),
  CMDTM_GEN(29, NA,    0, NA, 0),
  CMDTM_GEN(30, NA,    0, NA, 0),
  CMDTM_GEN(31, NA,    0, NA, 0),
  CMDTM_GEN(32, NA,    0, NA, 0),
  CMDTM_GEN(33, NA,    0, NA, 0),
  CMDTM_GEN(34, NA,    0, NA, 0),
  CMDTM_GEN(35, NA,    0, NA, 0),
  CMDTM_GEN(36, NA,    0, NA, 0),
  CMDTM_GEN(37, NA,    0, NA, 0),
  CMDTM_GEN(38, NA,    0, NA, 0),
  CMDTM_GEN(39, NA,    0, NA, 0),
  CMDTM_GEN(40, NA,    0, NA, 0),
  CMDTM_GEN(41, R3,    0, NA, 0),
  CMDTM_GEN(42, NA,    0, NA, 0),
  CMDTM_GEN(43, NA,    0, NA, 0),
  CMDTM_GEN(44, NA,    0, NA, 0),
  CMDTM_GEN(45, NA,    0, NA, 0),
  CMDTM_GEN(46, NA,    0, NA, 0),
  CMDTM_GEN(47, NA,    0, NA, 0),
  CMDTM_GEN(48, NA,    0, NA, 0),
  CMDTM_GEN(49, NA,    0, NA, 0),
  CMDTM_GEN(50, R1,    1, NA, 0),
  CMDTM_GEN(51, R1,    1, CARD_TO_HOST, 1),
  CMDTM_GEN(52, NA,    0, NA, 0),
  CMDTM_GEN(53, NA,    0, NA, 0),
  CMDTM_GEN(54, NA,    0, NA, 0),
  CMDTM_GEN(55, NA,    0, NA, 0),
  CMDTM_GEN(56, NA,    0, NA, 0),
  CMDTM_GEN(57, NA,    0, NA, 0),
  CMDTM_GEN(58, NA,    0, NA, 0),
  CMDTM_GEN(59, NA,    0, NA, 0)
};

static inline void OPTIMIZED bcm2835_emmc_cmd_process_single_block(
  char *buf,
  int size,
  int is_write,
  uint32_t intbits,
  uint64_t timeout_usec,
  bool blocking)
{
  uint32_t *ptr, *end;

  ptr = (uint32_t*)buf;
  end = ptr + (size / sizeof(*ptr));

  if (is_write) {
    while(ptr != end)
      ioreg32_write(BCM2835_EMMC_DATA, *ptr++);
  } else {
    while(ptr != end)
      *ptr++ = ioreg32_read(BCM2835_EMMC_DATA);
  }
}

static inline int bcm2835_emmc_report_interrupt_error(const char *tag,
  uint32_t intval)
{
  char buf[256];
  bcm2835_emmc_interrupt_bitmask_to_string(buf, sizeof(buf), intval);
  BCM2835_EMMC_ERR("%s: error bits in INTERRUPT register: %08x %s", intval, buf);
  return ERR_GENERIC;
}

static inline int bcm2835_emmc_wait_process_interrupts(const char *tag,
  uint32_t intbits,
  uint64_t timeout_usec,
  bool blocking)
{
  uint32_t intval;
  if (bcm2835_emmc_wait_reg_value(BCM2835_EMMC_INTERRUPT, 0, intbits, timeout_usec, blocking,
    &intval))
    return ERR_TIMEOUT;

  /* clear received interrupts */
  bcm2835_emmc_write_reg(BCM2835_EMMC_INTERRUPT, intval);
  if (BCM2835_EMMC_INTERRUPT_GET_ERR(intval)) {
    if (BCM2835_EMMC_INTERRUPT_GET_CTO_ERR(intval))
      return ERR_TIMEOUT;
    return bcm2835_emmc_report_interrupt_error(tag, intval);
  }
  return SUCCESS;
}

static inline int OPTIMIZED bcm2835_emmc_cmd_process_data(
  struct bcm2835_emmc_cmd *c,
  uint32_t cmdreg,
  uint64_t timeout_usec,
  bool blocking)
{
  int status;
  int block;
  bool is_write;
  uint32_t intbits;
  char *buf;

  /*
   * BCM2835_EMMC_INTERRUPT register works as follows:
   * 0xffff0000 - is a bitmask to error interrupt bits.
   * bit 15 - is ERR bit - means some error has happened.
   * bits through 16-24 - details of error, hence 0xffff0000 - is a mask to
   * all error types.
   * bit 0 - CMD_DONE. To check that command has been received by SD card, we
   * poll for CMD_DONE bit AND we also poll fro ERR bit at the same time. So,
   * that we can know weather the command has been completed or the error has
   * happened.
   * After CMD_DONE shows up we know command has been processed successfully.
   * If command also implies * DATA, then we need a couple of other interrupt
   * bits:
   *
   * bit 4 - WRITE_RDY - we poll for this bit if we want to write to data port.
   * At WRITE_RDY start writing to data port. The bit is set for each new block
   *
   * bit 5 - READ_RDY - we poll for this if we want to read some data from
   * data port.
   * At READ_RDY start fetching words from data port. The bit is set for each
   * new block
   *
   * bit 1 - DATA_DONE - controller sets this bit, when full amount of bytes,
   * ordered in BLOCKSIZELEN register, has been processed. Ex: block size = 8
   * and number of blocks = 2, so the total amount of data is 16 bytes. After
   * all 16 bytes have been processed by writing/reading them to/from data
   * port this bit will be set. Another logic would be to do data port IOs
   * until this bit is set, but I haven't tried it.
   */
  is_write = BCM2835_EMMC_CMDTM_GET_TM_DAT_DIR(cmdreg) ==
    BCM2835_EMMC_TRANS_TYPE_DATA_HOST_TO_CARD;

  /*
   * Decide which bits we should expect in the interrupt register.
   * for read  operations this is 0x8020 (bit 15 ERR, bit 4 READ_RDY )
   * for write operations this is 0x8010 (bit 15 ERR, bit 4 WRITE_RDY)
   */
  intbits = 0;
  BCM2835_EMMC_INTERRUPT_CLR_SET_ERR(intbits, 1);
  if (is_write) {
    /* 0x8010 */
    BCM2835_EMMC_INTERRUPT_CLR_SET_WRITE_RDY(intbits, 1);
  } else {
    /* 0x8020 */
    BCM2835_EMMC_INTERRUPT_CLR_SET_READ_RDY(intbits, 1);
  }

  for (block = 0; block < c->num_blocks; ++block) {
    status = bcm2835_emmc_wait_process_interrupts("emmc_cmd_process_data.block",
      intbits, timeout_usec, blocking);
    if (status != SUCCESS) {
      BCM2835_EMMC_ERR("error status during data ready wait: %d", status);
      return status;
    }

    buf = c->databuf + block * c->block_size;
    bcm2835_emmc_cmd_process_single_block(buf, c->block_size, is_write, intbits, timeout_usec, blocking);
  }

  intbits = 0;
  BCM2835_EMMC_INTERRUPT_CLR_SET_ERR(intbits, 1);
  BCM2835_EMMC_INTERRUPT_CLR_SET_DATA_DONE(intbits, 1);
  /* 0x8002 */
  return bcm2835_emmc_wait_process_interrupts("emmc_cmd_process_data.final",
    intbits, timeout_usec, blocking);
}

static inline int bcm2835_emmc_issue_cmd(struct bcm2835_emmc_cmd *c,
  uint32_t cmdreg, uint64_t timeout_usec, bool blocking)
{
  int data_status;
  int err;
  uint32_t blksizecnt;
  uint32_t intval, intval_cmp;
  int response_type;
  char intbuf[256];

#ifdef BCM2835_EMMC_DEBUG
  BCM2835_EMMC_DEBUG("emmc_issue_cmd: cmd_idx:%08x, arg:%08x, blocks:%d",
    c->cmd_idx, c->arg, c->num_blocks);
#endif

  if (bcm2835_emmc_wait_cmd_inhibit()) {
    BCM2835_EMMC_ERR("emmc_issue_cmd: bcm2835_emmc_wait_cmd_inhibit failed");
    return -1;
  }
  if (bcm2835_emmc_wait_dat_inhibit()) {
    BCM2835_EMMC_ERR("emmc_issue_cmd: bcm2835_emmc_wait_dat_inhibit failed");
    return -1;
  }

  blksizecnt = 0;
  BCM2835_EMMC_BLKSIZECNT_CLR_SET_BLKSIZE(blksizecnt, c->block_size);
  BCM2835_EMMC_BLKSIZECNT_CLR_SET_BLKCNT(blksizecnt, c->num_blocks);
  bcm2835_emmc_write_reg(BCM2835_EMMC_BLKSIZECNT, blksizecnt);
  bcm2835_emmc_write_reg(BCM2835_EMMC_ARG1, c->arg);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CMDTM, cmdreg);

  err = bcm2835_emmc_interrupt_wait_done_or_err(timeout_usec, 1, 0, blocking,
    &intval);

  if (err) {
    BCM2835_EMMC_ERR(
      "emmc_issue_cmd: emmc_interrupt_wait_done_or_err, err: %d", err);
    return ERR_TIMEOUT;
  }

  if (BCM2835_EMMC_INTERRUPT_GET_ERR(intval)) {
    bcm2835_emmc_write_reg(BCM2835_EMMC_INTERRUPT, intval);
    if (BCM2835_EMMC_INTERRUPT_GET_CTO_ERR(intval)) {
      intval_cmp = 0;
      BCM2835_EMMC_INTERRUPT_CLR_SET_CTO_ERR(intval_cmp, 1);
      BCM2835_EMMC_INTERRUPT_CLR_SET_ERR(intval_cmp, 1);
      if (intval_cmp == intval) {
        BCM2835_EMMC_ERR("emmc_issue_cmd: exit by timeout");
        return ERR_TIMEOUT;
      }
    }
    bcm2835_emmc_interrupt_bitmask_to_string(intbuf, sizeof(intbuf), intval);
    BCM2835_EMMC_ERR("emmc_issue_cmd: error in INTERRUPT register: %08x, %s", intval,
      intbuf);
    return ERR_GENERIC;
  }

  bcm2835_emmc_write_reg(BCM2835_EMMC_INTERRUPT, 0xffff0001);
  response_type = BCM2835_EMMC_CMDTM_GET_CMD_RSPNS_TYPE(cmdreg);

  switch(response_type) {
    case BCM2835_EMMC_RESPONSE_TYPE_NONE:
      break;
    case BCM2835_EMMC_RESPONSE_TYPE_136_BITS:
      c->resp0 = bcm2835_emmc_read_reg(BCM2835_EMMC_RESP0);
      c->resp1 = bcm2835_emmc_read_reg(BCM2835_EMMC_RESP1);
      c->resp2 = bcm2835_emmc_read_reg(BCM2835_EMMC_RESP2);
      c->resp3 = bcm2835_emmc_read_reg(BCM2835_EMMC_RESP3);
      break;
    case BCM2835_EMMC_RESPONSE_TYPE_48_BITS:
      c->resp0 = bcm2835_emmc_read_reg(BCM2835_EMMC_RESP0);
    break;
    case BCM2835_EMMC_RESPONSE_TYPE_48_BITS_BUSY:
      c->resp0 = bcm2835_emmc_read_reg(BCM2835_EMMC_RESP0);
    break;
  }

  if (BCM2835_EMMC_CMDTM_GET_CMD_ISDATA(cmdreg)) {
    data_status = bcm2835_emmc_cmd_process_data(c, cmdreg, timeout_usec,
      blocking);

    if (data_status != SUCCESS) {
      BCM2835_EMMC_ERR("emmc_issue_cmd: data_status: %d", data_status);
      return data_status;
    }
  }

  if (response_type == BCM2835_EMMC_RESPONSE_TYPE_48_BITS_BUSY) {
    ioreg32_read(BCM2835_EMMC_INTERRUPT);
    err = bcm2835_emmc_interrupt_wait_done_or_err(timeout_usec, 0, 1,
      blocking, &intval);

    if (err)
      return ERR_TIMEOUT;

    bcm2835_emmc_write_reg(BCM2835_EMMC_INTERRUPT, 0xffff0002);
  }
#ifdef BCM2835_EMMC_DEBUG
  BCM2835_EMMC_DEBUG("emmc_issue_cmd result: %d, resp: [%08x][%08x][%08x][%08x]",
    c->status,
    c->resp0,
    c->resp1,
    c->resp2,
    c->resp3);
#endif
  return SUCCESS;
}

int bcm2835_emmc_cmd(struct bcm2835_emmc_cmd *c, uint64_t timeout_usec,
  bool blocking)
{
  uint32_t intval;
  char intbuf[256];
  int tmp_status;
  int acmd_idx;
  struct bcm2835_emmc_cmd tmp_cmd;

  intval = bcm2835_emmc_read_reg(BCM2835_EMMC_INTERRUPT);
  if (intval) {
    bcm2835_emmc_interrupt_bitmask_to_string(intbuf, sizeof(intbuf), intval);
    BCM2835_EMMC_WARN("interrupts detected: %08x, %s", intval, intbuf);
    bcm2835_emmc_write_reg(BCM2835_EMMC_INTERRUPT, intval);
  }

  if (BCM2835_EMMC_CMD_IS_ACMD(c->cmd_idx)) {
    bcm2835_emmc_cmd_init(&tmp_cmd, BCM2835_EMMC_CMD55 /* APP_CMD */,
      c->rca << 16);

    tmp_cmd.num_blocks = c->num_blocks;
    tmp_cmd.block_size = c->block_size;

    tmp_status = bcm2835_emmc_issue_cmd(&tmp_cmd,
      sd_commands[BCM2835_EMMC_CMD55], timeout_usec, blocking);

    if (tmp_status != SUCCESS)
      return tmp_status;

    acmd_idx = BCM2835_EMMC_ACMD_RAW_IDX(c->cmd_idx);
    return bcm2835_emmc_issue_cmd(c, sd_acommands[acmd_idx], timeout_usec,
      blocking);
  }

  return bcm2835_emmc_issue_cmd(c, sd_commands[c->cmd_idx], timeout_usec,
    blocking);
}

int bcm2835_emmc_reset_cmd(bool blocking)
{
  uint32_t control1;

  control1 = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
  BCM2835_EMMC_CONTROL1_CLR_SET_SRST_CMD(control1, 1);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL1, control1);

  if (bcm2835_emmc_wait_reg_value(BCM2835_EMMC_CONTROL1,
    BCM2835_EMMC_CONTROL1_MASK_SRST_CMD, 0, BCM2835_EMMC_WAIT_TIMEOUT_USEC,
    blocking, NULL)) {
    BCM2835_EMMC_ERR("emmc_reset_cmd: timeout");
    return ERR_TIMEOUT;
  }

  return SUCCESS;
}

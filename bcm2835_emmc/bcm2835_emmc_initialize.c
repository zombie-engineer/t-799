/*
 * SD card initialization procedure.
 * Documentation:
 * 1. SD Card Initialization Part 1-2-3 http://www.rjhcoding.com/avrc-sd-interface-1.php
 */

#include <mbox.h>
#include <mbox_props.h>
#include <bitops.h>
#include <delay.h>
#include <bcm2835/bcm2835_emmc.h>
#include "bcm2835_emmc_cmd.h"
#include "bcm2835_emmc_utils.h"

#define BCM2835_EMMC_CHECK_ERR(__fmt, ...)\
  do {\
    if (err != SUCCESS) {\
      BCM2835_EMMC_ERR("%s err %d, " __fmt, __func__,  err, ## __VA_ARGS__);\
      return err;\
    }\
  } while(0)

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

/* SEND_RELATIVE_ADDR */
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

  BCM2835_EMMC_LOG("bcm2835_emmc_cmd3 result: err: %d, crc_err: %d,"
    " illegal_cmd: %d, status: %d, ready: %d, rca: %04x",
    error, crc_error, illegal_cmd, status, ready, rca);

  if (error)
    return ERR_GENERIC;

  *out_rca = rca;

  return SUCCESS;
}

static inline int bcm2835_emmc_cmd5(bool blocking)
{
  struct bcm2835_emmc_cmd c;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD5, 0);
  return bcm2835_emmc_cmd(&c, 2000, blocking);
}

static inline int bcm2835_emmc_cmd8(bool blocking)
{
  int cmd_ret;
  struct bcm2835_emmc_cmd c;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD8, BCM2835_EMMC_CMD8_ARG);
  cmd_ret = bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);

  if (cmd_ret != SUCCESS)
    return cmd_ret;

  if (c.resp0 != BCM2835_EMMC_CMD8_VALID_RESP)
    return ERR_GENERIC;

  return SUCCESS;
}

static inline int bcm2835_emmc_acmd6(uint32_t rca, uint32_t bus_width_bit,
  bool blocking)
{
  struct bcm2835_emmc_cmd c;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_ACMD6, bus_width_bit);
  c.rca = rca;
  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);
}

static inline int bcm2835_emmc_acmd41(uint32_t arg, uint32_t *resp,
  bool blocking)
{
  struct bcm2835_emmc_cmd c;
  int cmd_ret;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_ACMD41, arg);
  cmd_ret = bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC, blocking);

  if (cmd_ret == SUCCESS)
    *resp = c.resp0;
  return cmd_ret;
}

/* SEND_SCR */
static inline int bcm2835_emmc_acmd51(uint32_t rca, char *scrbuf,
  int scrbuf_len, bool blocking)
{
  struct bcm2835_emmc_cmd c;

  if (scrbuf_len < 8) {
    BCM2835_EMMC_CRIT("SRC buffer less than 8 bytes");
    return -1;
  }

  if ((uint64_t)scrbuf & 3) {
    BCM2835_EMMC_CRIT("SRC buffer not aligned to 4 bytes");
    return -1;
  }

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_ACMD51, 0);

  c.block_size = 8;
  c.num_blocks = 1;
  c.rca = rca;
  c.databuf = scrbuf;

  return bcm2835_emmc_cmd(&c, BCM2835_EMMC_WAIT_TIMEOUT_USEC * 4, blocking);
}

static inline void bcm2835_emmc_set_block_size(uint32_t block_size)
{
  uint32_t regval;
  regval = bcm2835_emmc_read_reg(BCM2835_EMMC_BLKSIZECNT);
  regval &= 0xffff;
  regval |= block_size;
  bcm2835_emmc_write_reg(BCM2835_EMMC_BLKSIZECNT, regval);
}

static inline int bcm2835_emmc_reset_handle_scr(uint32_t rca, uint32_t *scr,
  bool blocking)
{
  int err;
  char *s = (char *)scr;
  uint32_t control0;
  uint32_t scr_le32 = (s[0]<<24)| (s[1]<<16) | (s[2]<<8) | s[3];
  int sd_spec   = BITS_EXTRACT32(scr_le32, (56-32), 4);
  int sd_spec3  = BITS_EXTRACT32(scr_le32, (47-32), 1);
  int sd_spec4  = BITS_EXTRACT32(scr_le32, (42-32), 1);
  int sd_specx  = BITS_EXTRACT32(scr_le32, (38-32), 4);
  int bus_width = BITS_EXTRACT32(scr_le32, (48-32), 4);

  BCM2835_EMMC_LOG("SCR: sd_spec:%d, sd_spec3:%d, sd_spec4:%d, sd_specx:%d",
    sd_spec, sd_spec3, sd_spec4, sd_specx);
  if (bus_width & 4) {
    err = bcm2835_emmc_acmd6(rca, BCM2835_EMMC_BUS_WIDTH_4BITS, blocking);
    BCM2835_EMMC_CHECK_ERR("failed to set bus to 4 bits");

    control0 = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL0);
    BCM2835_EMMC_CONTROL0_CLR_SET_HCTL_DWIDTH(control0, 1);
    bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL0, control0);
  }
  bcm2835_emmc_write_reg(BCM2835_EMMC_INTERRUPT, 0xffffffff);
  return SUCCESS;
}

int bcm2835_emmc_reset(bool blocking, uint32_t *rca, uint32_t *device_id)
{
  int i, err;
  uint32_t status;
  uint32_t powered_on = 0, exists = 0;
  uint32_t control1;
  uint32_t intmsk;
  uint32_t acmd41_resp;
  uint32_t bcm2835_emmc_status;
  uint32_t bcm2835_emmc_state;
  uint32_t scr[2];

#if 0
  irq_set(BCM2835_IRQNR_ARASAN_SDIO, bcm2835_emmc_irq_handler);
  bcm2835_ic_enable_irq(BCM2835_IRQNR_ARASAN_SDIO);
#endif
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL0, 0);

  if (!mbox_get_power_state(MBOX_DEVICE_ID_SD, &powered_on, &exists))
    BCM2835_EMMC_ERR("bcm2835_emmc_reset: failed to get SD power state");

  BCM2835_EMMC_LOG("bcm2835_emmc_reset: SD powered on: %d, exists: %d",
    powered_on, exists);

  powered_on = 1;
  uint32_t clock_rate = 1;
  uint32_t min_clock_rate = 1;
  uint32_t max_clock_rate = 1;
  mbox_get_clock_rate(MBOX_CLOCK_ID_EMMC, &clock_rate);
  mbox_get_min_clock_rate(MBOX_CLOCK_ID_EMMC, &min_clock_rate);
  mbox_get_max_clock_rate(MBOX_CLOCK_ID_EMMC, &max_clock_rate);
  BCM2835_EMMC_LOG("clock rate: %d/%d/%d", clock_rate, min_clock_rate, max_clock_rate);

  /* Disable and re-set clock */
  /* 1. Reset circuit and disable clock */
  control1 = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
  BCM2835_EMMC_CONTROL1_CLR_SET_SRST_HC(control1, 1);
  BCM2835_EMMC_CONTROL1_CLR_CLK_INTLEN(control1);
  BCM2835_EMMC_CONTROL1_CLR_CLK_EN(control1);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL1, control1);

  for(i = 0; i < 1000; ++i) {
    delay_us(6);
    control1 = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
    if (!BCM2835_EMMC_CONTROL1_GET_SRST_HC(control1))
      break;
  }

  if (i == 1000) {
    BCM2835_EMMC_ERR("SRST_HC timeout");
    return ERR_TIMEOUT;
  }

  BCM2835_EMMC_LOG("bcm2835_emmc_reset: after SRST_HC: control1: %08x",
    control1);

  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL2, 0);

  /* Set low clock */
  err = bcm2835_emmc_set_clock(BCM2835_EMMC_CLOCK_HZ_SETUP);
  if (err != SUCCESS) {
    BCM2835_EMMC_ERR("failed to set clock to %d Hz",
      BCM2835_EMMC_CLOCK_HZ_SETUP);
    return err;
  }

  status = bcm2835_emmc_read_reg(BCM2835_EMMC_STATUS);
  BCM2835_EMMC_LOG("STATUS: %08x", status);

  BCM2835_EMMC_LOG("Enabling SD clock");
  delay_us(2000);
  control1 = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
  BCM2835_EMMC_CONTROL1_CLR_SET_CLK_EN(control1, 1);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL1, control1);
  delay_us(2000);

  bcm2835_emmc_write_reg(BCM2835_EMMC_IRPT_EN, 0xffffffff);
  intmsk = 0xffffffff;
  bcm2835_emmc_write_reg(BCM2835_EMMC_INTERRUPT, intmsk);
  // BCM2835_EMMC_INTERRUPT_CLR_CARD(intmsk);
  intmsk = 0xfffffeff;
  bcm2835_emmc_write_reg(BCM2835_EMMC_IRPT_MASK, intmsk);
  delay_us(2000);

  /* CMD0 - GO_IDLE_STATE - reset device to idle state, no response */
  err = bcm2835_emmc_cmd0(blocking);
  BCM2835_EMMC_CHECK_ERR("Failed at CMD0 (GO_TO_IDLE)");

  /* CMD8 - SEND_NEXT_CSD - Device sends all EXT_CSD  register */
  err = bcm2835_emmc_cmd8(blocking);
  BCM2835_EMMC_CHECK_ERR("Failed at CMD8 (SEND_CSD)");

  /* 
   * CMD5 is CHECK_SDIO command, it will timeout for non-SDIO devices
   */
  err = bcm2835_emmc_cmd5(blocking);
  if (err == SUCCESS) {
    BCM2835_EMMC_CRIT("bcm2835_emmc_reset: detected SDIO card. Not supported");
    return ERR_GENERIC;
  } 
  if (err != ERR_TIMEOUT) {
    BCM2835_EMMC_ERR("bcm2835_emmc_reset: failed at CMD5 step, err: %d", err);
    return err;
  }

  BCM2835_EMMC_LOG("bcm2835_emmc_reset: detected SD card");
  /*
   * After failed command (timeout) we should reset command state machine 
   * to a known state.
   */
  err = bcm2835_emmc_reset_cmd(blocking);
  BCM2835_EMMC_CHECK_ERR("failed to reset command after SDIO_CHECK");

  err = bcm2835_emmc_acmd41(0, &acmd41_resp, blocking);
  BCM2835_EMMC_CHECK_ERR("failed at ACMD41");

  while(BIT_IS_CLEAR(acmd41_resp, 31)) {
    bcm2835_emmc_acmd41(0x00ff8000 | (1<<28) | (1<<30), &acmd41_resp,
      blocking);
    delay_us(500000);
  }

  /* Set normal clock */
  err = bcm2835_emmc_set_clock(BCM2835_EMMC_CLOCK_HZ_NORMAL);
  BCM2835_EMMC_CHECK_ERR("failed to set clock to %d Hz",
    BCM2835_EMMC_CLOCK_HZ_NORMAL);

  /* Get device id */
  err = bcm2835_emmc_cmd2(device_id, blocking);
  BCM2835_EMMC_CHECK_ERR("failed at CMD2 (SEND_ALL_CID) step");

  BCM2835_EMMC_LOG("bcm2835_emmc_reset: device_id: %08x.%08x.%08x.%08x",
    device_id[0], device_id[1], device_id[2], device_id[3]);

  err = bcm2835_emmc_cmd3(rca, blocking);
  BCM2835_EMMC_CHECK_ERR("failed at CMD3 (SEND_RELATIVE_ADDR) step");
  BCM2835_EMMC_LOG("bcm2835_emmc_reset: RCA: %08x", *rca);

  err = bcm2835_emmc_cmd7(*rca, blocking);
  BCM2835_EMMC_CHECK_ERR("failed at CMD7 (SELECT_CARD) step");

  err = bcm2835_emmc_cmd13(*rca, &bcm2835_emmc_status, blocking);
  BCM2835_EMMC_CHECK_ERR("failed at CMD13 (SEND_STATUS) step");

  bcm2835_emmc_state = BCM2835_EMMC_CARD_STATUS_GET_CURRENT_STATE(
    bcm2835_emmc_status);

  BCM2835_EMMC_LOG("bcm2835_emmc_reset: status: %08x, curr state: %d(%s)",
    bcm2835_emmc_status, bcm2835_emmc_state,
    bcm2835_emmc_state_to_string(bcm2835_emmc_state));

  bcm2835_emmc_set_block_size(BCM2835_EMMC_BLOCK_SIZE);

  err = bcm2835_emmc_acmd51(*rca, (char *)scr, sizeof(scr),
    blocking);

  BCM2835_EMMC_CHECK_ERR("failed at ACMD51 (SEND_SCR) step");

  BCM2835_EMMC_LOG("bcm2835_emmc_reset: SCR: %08x.%08x", scr[0], scr[1]);

  err = bcm2835_emmc_reset_handle_scr(*rca, scr, blocking);
  BCM2835_EMMC_CHECK_ERR("failed at SCR handling step");

  BCM2835_EMMC_LOG("bcm2835_emmc_reset: completed successfully");

  return SUCCESS;
}

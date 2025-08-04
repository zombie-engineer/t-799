/*
 * SD card initialization procedure.
 * Documentation:
 * 1. SD Card Initialization Part 1-2-3 http://www.rjhcoding.com/avrc-sd-interface-1.php
 */

#include <mbox.h>
#include <mbox_props.h>
#include <bitops.h>
#include <delay.h>
#include <printf.h>
#include <bcm2835/bcm2835_emmc.h>
#include "bcm2835_emmc_cmd.h"
#include "bcm2835_emmc_utils.h"
#include <sdhc.h>
#include <printf.h>

#define BCM2835_EMMC_CHECK_ERR(__fmt, ...)\
  do {\
    if (err != SUCCESS) {\
      BCM2835_EMMC_ERR("%s err %d, " __fmt, __func__,  err, ## __VA_ARGS__);\
      return err;\
    }\
  } while(0)


static inline void bcm2835_emmc_set_block_size(uint32_t block_size)
{
  uint32_t regval;
  regval = bcm2835_emmc_read_reg(BCM2835_EMMC_BLKSIZECNT);
  regval &= 0xffff;
  regval |= block_size;
  bcm2835_emmc_write_reg(BCM2835_EMMC_BLKSIZECNT, regval);
}

static inline void bcm2835_sdhc_set_bus_width4(void)
{
  uint32_t v = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL0);
  BCM2835_EMMC_CONTROL0_CLR_SET_HCTL_DWIDTH(v, 1);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL0, v);
}

static void bcm2835_emmc_set_high_speed(void)
{
  uint32_t v = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL0);
  BCM2835_EMMC_CONTROL0_CLR_SET_HCTL_HS_EN(v, 1);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL0, v);
}

#if 0
static inline int bcm2835_sd_parse_scr(uint64_t scr, bool *data_width4)
{
  int sd_spec_ver_maj;
  int sd_spec_ver_min;


  int err;
  bool buswidth4_supported = sd_scr_bus_width4_supported(scr);

  sd_scr_get_sd_spec_version(scr, &sd_spec_ver_maj, &sd_spec_ver_min);

  BCM2835_EMMC_LOG("SCR: sd spec version: %d.%d\n"
    "bus_width1: %s, buswidth4:%s",
    sd_spec_ver_maj,
    sd_spec_ver_min,
    sd_scr_bus_width1_supported(scr) ? "supported" : "not_supp",
    buswidth4_supported ? "supported" : "not_supp"
  );

  *data_width4 = buswidth4_supported;

#if 0
  printf("Tuning CONTROL2, interrupt:%08x\r\n", ioreg32_read(BCM2835_EMMC_INTERRUPT));

  uint32_t ctrl2 =  ioreg32_read(BCM2835_EMMC_CONTROL2);

  ctrl2 |= 1<<22;
  ctrl2 |= 1 << 16;

  ioreg32_write(BCM2835_EMMC_CONTROL2, ctrl2);
#endif
  uint32_t rrr = ioreg32_read(BCM2835_EMMC_CONTROL1);
  printf("CONTROL1: %08x\r\n", rrr);
  rrr |= (0xf << 16) | (1<<24);
  ioreg32_write(BCM2835_EMMC_CONTROL2, rrr);
#if 0
  for (int i =0 ; i < 1000; ++i) {
    printf("control2:%08x interrupt:%08x\r\n", ioreg32_read(BCM2835_EMMC_CONTROL2), ioreg32_read(BCM2835_EMMC_INTERRUPT));
  }

  while(1) {
    asm volatile ("wfe");
  }
  bcm2835_emmc_write_reg(BCM2835_EMMC_INTERRUPT, 0xffffffff);
#endif
  BCM2835_EMMC_LOG("Switched to SDR25: 50MHz clock, 25 megabytes/sec\r\n");
  return SUCCESS;
}
#endif

static void bcm2835_emmc_dump_mbox_info(void)
{
  uint32_t powered_on = 0;
  uint32_t exists = 0;
  uint32_t clock_rate = 0;
  uint32_t min_clock_rate = 0;
  uint32_t max_clock_rate = 0;

  if (!mbox_get_power_state(MBOX_DEVICE_ID_SD, &powered_on, &exists))
    BCM2835_EMMC_ERR("failed to get SD power state");

  BCM2835_EMMC_LOG("SD powered on: %d, exists: %d", powered_on, exists);

  if (!mbox_get_clock_rate(MBOX_CLOCK_ID_EMMC, &clock_rate))
    BCM2835_EMMC_ERR("failed to get EMMC clock rate");

  if (!mbox_get_min_clock_rate(MBOX_CLOCK_ID_EMMC, &min_clock_rate))
    BCM2835_EMMC_ERR("failed to get EMMC min clock rate");

  if (!mbox_get_max_clock_rate(MBOX_CLOCK_ID_EMMC, &max_clock_rate))
    BCM2835_EMMC_ERR("failed to get EMMC max clock rate");

  BCM2835_EMMC_LOG("clock rate: %d/%d/%d", clock_rate, min_clock_rate,
    max_clock_rate);
}

static void bcm2835_emmc_interrupt_initialize(void)
{
  uint32_t intmsk;
  /* Reset and enable interrupts - not sure this code is ok */
  bcm2835_emmc_write_reg(BCM2835_EMMC_IRPT_EN, 0xffffffff);
  intmsk = 0xffffffff;
  bcm2835_emmc_write_reg(BCM2835_EMMC_INTERRUPT, intmsk);
  BCM2835_EMMC_INTERRUPT_CLR_CARD(intmsk);
  bcm2835_emmc_write_reg(BCM2835_EMMC_IRPT_MASK, intmsk);
  delay_us(2000);
}

static void bcm2835_sdhc_full_reset(void)
{
  /* Set bit 24 to reset CMD and DATA state machines */
  uint32_t v = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
  BCM2835_EMMC_CONTROL1_CLR_SET_SRST_HC(v, 1);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL1, v);

  while(1) {
    v = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
    if (!BCM2835_EMMC_CONTROL1_GET_SRST_HC(v))
      break;
  }
}

/* Wait until commincation with card is over */
static void bcm2835_sdhc_wait_card_busy(void)
{
  uint32_t v;
  uint32_t mask = 0;

  /* Prepare mask that indicates CMD and DATA lines are busy */
  BCM2835_EMMC_STATUS_CLR_SET_CMD_INHIBIT(mask, 1);
  BCM2835_EMMC_STATUS_CLR_SET_DAT_INHIBIT(mask, 1);
  v = mask;

  while (v & mask)
    v = bcm2835_emmc_read_reg(BCM2835_EMMC_STATUS);
}

static void bcm2835_sdhc_sd_clk_stop(void)
{
  uint32_t v;
  bcm2835_sdhc_wait_card_busy();
  v = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
  BCM2835_EMMC_CONTROL1_CLR_CLK_EN(v);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL1, v);
}

static void bcm2835_sdhc_sd_clk_start(void)
{
  uint32_t v = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
  BCM2835_EMMC_CONTROL1_CLR_SET_CLK_EN(v, 1);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL1, v);
  while(1) {
    v = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
    if (BCM2835_EMMC_CONTROL1_GET_CLK_STABLE(v))
      break;
  }
}

static void bcm2835_sdhc_internal_clk_stop(void)
{
  uint32_t v = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
  BCM2835_EMMC_CONTROL1_CLR_CLK_INTLEN(v);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL1, v);

  while(1) {
    v = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
    if (!BCM2835_EMMC_CONTROL1_GET_CLK_STABLE(v))
      break;
  }
}

static void bcm2835_sdhc_internal_clk_start(void)
{
  uint32_t v = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
  BCM2835_EMMC_CONTROL1_CLR_SET_CLK_INTLEN(v, 1);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL1, v);

  while(1) {
    v = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
    if (BCM2835_EMMC_CONTROL1_GET_CLK_STABLE(v))
      break;
  }
}

static int bcm2835_sdhc_run_acmd41(bool blocking)
{
  int err;
  uint32_t arg;
  uint32_t resp = 0;

  /* Host supports HIGH CAPACITY (SDHC/SDXC) */
  #define ACMD41_RESP_BUSY (1<<31)

  #define ACMD41_ARG_HCS  (1<<30)
  /* SDXC power saving - max performance enable */
  #define ACMD41_ARG_XPC  (1<<28)
  #define ACMD41_ARG_S18R (1<<24)
  #define ACMD41_ARG_VDD  0

  err = bcm2835_emmc_acmd41(0, &resp, blocking);
  BCM2835_EMMC_CHECK_ERR("ACMD41 (SD_SEND_OP_COND) failed");
  BCM2835_EMMC_LOG("ACMD41: arg:%08x, resp:%08x", 0, resp);

  arg = (resp & 0x00ff8000) | ACMD41_ARG_HCS;
  err = bcm2835_emmc_acmd41(arg, &resp, blocking);
  BCM2835_EMMC_CHECK_ERR("ACMD41 (SD_SEND_OP_COND) failed");

  arg = 0x40ff8000;
  resp = 0;

  while (!(resp & ACMD41_RESP_BUSY)) {
    err = bcm2835_emmc_acmd41(arg, &resp, blocking);
    BCM2835_EMMC_CHECK_ERR("ACMD41 (SD_SEND_OP_COND) failed 2");
  }

  return SUCCESS;
}

#define LOW_FREQ false
#define HIGH_FREQ true

static void bcm2835_sdhc_set_timeout(void)
{
  uint32_t v = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
  /* Timeout = CLK x 2^(13+bitsvalue).= CLK x 2^(13 + 0xb) = CLK x 2^24 */
  BCM2835_EMMC_CONTROL1_CLR_SET_DATA_TOUNIT(v, 0xb);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL1, v);
}

static void bcm2835_sdhc_set_clk_div(uint32_t div)
{
  uint32_t v = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
  BCM2835_EMMC_CONTROL1_CLR_SET_CLK_FREQ8(v, div);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL1, v);
}

static void bcm2835_sdhc_clk_reconfigure(bool speed_normal)
{
  uint32_t div = speed_normal ? 5 : 64;

  bcm2835_sdhc_sd_clk_stop();
  bcm2835_sdhc_internal_clk_stop();
  bcm2835_sdhc_set_timeout();
  bcm2835_sdhc_set_clk_div(div);
  bcm2835_sdhc_internal_clk_start();
  bcm2835_sdhc_sd_clk_start();
}

static void bcm2835_sdhc_reset(void)
{
  bcm2835_sdhc_full_reset();
  bcm2835_emmc_read_reg(BCM2835_EMMC_STATUS);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL0, 0);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL2, 0);
  bcm2835_emmc_read_reg(BCM2835_EMMC_STATUS);

  /*
   * After reset SDHC clock speed should be low for capability with cards
   * that might be slow on initial phase
   */
  bcm2835_sdhc_clk_reconfigure(LOW_FREQ);
  bcm2835_emmc_interrupt_initialize();
  bcm2835_emmc_read_reg(BCM2835_EMMC_STATUS);
}

static int bcm2835_sdhc_identification_mode(uint32_t *rca, bool blocking)
{
  /*
   * Taken from "Physical Layer Simplified Specification Version 9.00"
   * SD host at initialization is in identification mode to know which cards
   * are available. In this mode it queries cards and knows their addresses
   * During that SD cards transition states in a defined order.
   * We want to set the card to STANDBY state and move SD host to data
   * transfer state.
   * For that we:
   * 1. Reset SD card back to IDLE state CMD0.
   * 2. Issue commands to traverse states:
   *    CDM0->IDLE->CMD8,CMD41->READY->CMD2->IDENT->CMD3->STANDBY
   */
  int err;
  sd_card_state_t card_state;
  uint32_t bcm2835_emmc_status;
  uint32_t device_id[4];

  bcm2835_emmc_read_reg(BCM2835_EMMC_STATUS);
  err = bcm2835_emmc_cmd0(blocking);
  BCM2835_EMMC_CHECK_ERR("Failed at CMD0 (GO_TO_IDLE)");
  /* card_state = IDLE */

  /* CMD8 -SEND_IF_COND - this checks if SD card supports our voltage */
  err = bcm2835_emmc_cmd8(blocking);
  BCM2835_EMMC_CHECK_ERR("CMD8 (SEND_IF_COND) failed");
  /* card_state = IDLE */

  printf("will run ACMD41\r\n");
  err = bcm2835_sdhc_run_acmd41(blocking);
  if (err != SUCCESS)
    return err;
  /* card_state = READY */

  /* Get device id */
  err = bcm2835_emmc_cmd2(device_id, blocking);
  BCM2835_EMMC_CHECK_ERR("failed at CMD2 (SEND_ALL_CID) step");
  /* card_state = IDENT */

  BCM2835_EMMC_LOG("bcm2835_emmc_reset: device_id: %08x.%08x.%08x.%08x",
    device_id[0], device_id[1], device_id[2], device_id[3]);

  err = bcm2835_emmc_cmd3(rca, blocking);
  BCM2835_EMMC_CHECK_ERR("failed at CMD3 (SEND_RELATIVE_ADDR) step");
  BCM2835_EMMC_LOG("bcm2835_emmc_reset: RCA: %08x", *rca);
  /* card_state = STANDBY */

  err = bcm2835_emmc_cmd13(*rca, &bcm2835_emmc_status, blocking);
  BCM2835_EMMC_CHECK_ERR("failed at CMD13 (SEND_STATUS) step");

  card_state = (bcm2835_emmc_status >> 9) & 0xf;
  if (card_state != SD_CARD_STATE_STANDBY) {
    BCM2835_EMMC_ERR("Card not in STANDBY state after init");
    return ERR_GENERIC;
  }

  return SUCCESS;
}

static int bcm2835_sdhc_data_transfer_mode(uint32_t rca, bool blocking)
{
  int err;
  bool data_width4_supported;

  struct sd_cmd6_response_raw r = { 0 };

  char scrbuf[8] = { 0 };
  char scrbuf_le[8] = { 0 };
  uint64_t scr;

  err = bcm2835_emmc_cmd9(rca, blocking);
  BCM2835_EMMC_CHECK_ERR("CMD9 (SEND_CSD) failed");

  err = bcm2835_emmc_cmd7(rca, blocking);
  BCM2835_EMMC_CHECK_ERR("CMD7 (SELECT_CARD) failed");
  /* card_state = TRAN */

  err = bcm2835_emmc_acmd51(rca, scrbuf, sizeof(scrbuf), blocking);
  BCM2835_EMMC_CHECK_ERR("ACMD51 (SEND_SCR) failed");

#if 0
  for (int i = 0; i < 8; ++i)
    scrbuf_le[i] = scrbuf[7 - i];

  err = bcm2835_sd_parse_scr(rca, scr, blocking, &data_width4_supported);
#endif

  data_width4_supported = true;
  if (data_width4_supported) {
    err = bcm2835_emmc_acmd6(rca, BCM2835_EMMC_BUS_WIDTH_4BITS, blocking);
    BCM2835_EMMC_CHECK_ERR("failed to set bus to 4 bits");
    bcm2835_sdhc_set_bus_width4();
    BCM2835_EMMC_LOG("SDHC configured for 4bit DAT");
  }

  err = bcm2835_emmc_cmd6(CMD6_MODE_CHECK,
    CMD6_ARG_ACCESS_MODE_SDR25,
    CMD6_ARG_CMD_SYSTEM_DEFAULT,
    CMD6_ARG_DRIVER_STRENGTH_DEFAULT,
    CMD6_ARG_POWER_LIMIT_DEFAULT,
    r.data,
    blocking);
  BCM2835_EMMC_CHECK_ERR("CMD6");

  BCM2835_EMMC_LOG("CMD6 result: max_current:%d, fns(%04x,%04x,%04x,%04x)\r\n",
    sd_cmd6_mode_0_resp_get_max_current(&r),
    sd_cmd6_mode_0_resp_get_supp_fns_1(&r),
    sd_cmd6_mode_0_resp_get_supp_fns_2(&r),
    sd_cmd6_mode_0_resp_get_supp_fns_3(&r),
    sd_cmd6_mode_0_resp_get_supp_fns_4(&r));

  if (!sd_cmd6_mode_0_resp_is_sdr25_supported(&r)) {
    BCM2835_EMMC_LOG("Switching to SDR25 not supported, skipping...\r\n");
    return SUCCESS;
  }

  BCM2835_EMMC_LOG("Switching to SDR25 ...\r\n");

  err = bcm2835_emmc_cmd6(CMD6_MODE_SWITCH,
    CMD6_ARG_ACCESS_MODE_SDR25,
    CMD6_ARG_CMD_SYSTEM_DEFAULT,
    CMD6_ARG_DRIVER_STRENGTH_DEFAULT,
    CMD6_ARG_POWER_LIMIT_DEFAULT,
    r.data,
    blocking);
  BCM2835_EMMC_CHECK_ERR("CMD6");

  bcm2835_emmc_set_high_speed();
  bcm2835_sdhc_clk_reconfigure(HIGH_FREQ);
  return SUCCESS;
}

int bcm2835_emmc_reset(bool blocking, uint32_t *rca, uint32_t *device_id)
{
  int err;
  uint32_t bcm2835_emmc_state;

  bcm2835_emmc_dump_mbox_info();
  bcm2835_sdhc_reset();
  err = bcm2835_sdhc_identification_mode(rca, blocking);
  if (err != SUCCESS)
    return err;

  err = bcm2835_sdhc_data_transfer_mode(*rca, blocking);
  if (err != SUCCESS)
    return err;

  BCM2835_EMMC_LOG("bcm2835_emmc_reset done");
  return SUCCESS;
}

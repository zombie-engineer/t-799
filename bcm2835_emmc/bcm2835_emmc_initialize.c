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
#include "sd.h"
#include <printf.h>

#define BCM2835_EMMC_CHECK_ERR(__fmt, ...)\
  do {\
    if (err != SUCCESS) {\
      BCM2835_EMMC_ERR("%s err %d, " __fmt, __func__,  err, ## __VA_ARGS__);\
      return err;\
    }\
  } while(0)


/* SEND_RELATIVE_ADDR */
static inline int bcm2835_emmc_cmd5(bool blocking)
{
  struct bcm2835_emmc_cmd c;

  bcm2835_emmc_cmd_init(&c, BCM2835_EMMC_CMD5, 0);
  return bcm2835_emmc_cmd(&c, 2000, blocking);
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

  BCM2835_EMMC_LOG("ACMD41 full response: %08x|%08x|%08x|%08x\r\n",
    c.resp0, c.resp1, c.resp2, c.resp3);

  if (cmd_ret == SUCCESS)
    *resp = c.resp0;
  return cmd_ret;
}

/* SEND_SCR */
static inline int bcm2835_emmc_acmd51(uint32_t rca, char *scrbuf,
  int scrbuf_len, bool blocking)
{
  struct bcm2835_emmc_cmd c = { 0 };

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

static int bcm2835_emmc_set_bus_width4(uint32_t rca, bool blocking)
{
  uint32_t control0;
  int err;
  err = bcm2835_emmc_acmd6(rca, BCM2835_EMMC_BUS_WIDTH_4BITS, blocking);

  printf("%08x\r\n", *(volatile uint32_t *)(0x3f30003c));

  BCM2835_EMMC_CHECK_ERR("failed to set bus to 4 bits");

  control0 = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL0);
  BCM2835_EMMC_CONTROL0_CLR_SET_HCTL_DWIDTH(control0, 1);
  BCM2835_EMMC_LOG("Enable bus width 4: setting CONTROL0 reg to %08x",
    control0);

  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL0, control0);

  return SUCCESS;
}

static void bcm2835_emmc_set_high_speed(void)
{
  uint32_t control1;
  uint32_t control0;
  uint32_t caps0 = bcm2835_emmc_read_reg(BCM2835_EMMC_CAPABILITIES_0);
  uint32_t caps1 = bcm2835_emmc_read_reg(BCM2835_EMMC_CAPABILITIES_1);
  uint32_t control2 = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL2);
  uint64_t allcaps = ((uint64_t)caps1 << 32) | caps0;
  control1 = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
  BCM2835_EMMC_CONTROL1_CLR_CLK_EN(control1);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL1, control1);

  BCM2835_EMMC_LOG("CAPS: %016x, %08x\r\n", allcaps, control2);
  control0 = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL0);
  BCM2835_EMMC_CONTROL0_CLR_SET_HCTL_HS_EN(control0, 1);
  BCM2835_EMMC_LOG("Enable high speed: setting CONTROL0 reg to %08x",
    control0);

  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL0, control0);

  control1 = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
  BCM2835_EMMC_CONTROL1_CLR_SET_CLK_EN(control1, 1);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL1, control1);
}

static void custom_cmd9(int rca)
{
  int status;
  uint32_t mask;

  printf("custom_cmd9, rca:%04x\r\n", rca);
  mask = BCM2835_EMMC_STATUS_MASK_CMD_INHIBIT |
    BCM2835_EMMC_STATUS_MASK_DAT_INHIBIT;

  while(1) {
    status = ioreg32_read(BCM2835_EMMC_STATUS);
    if (!(status & mask))
      break;

    BCM2835_EMMC_LOG("CMD9 wait inhibit");
    delay_us(100);
  }

  printf("before: rsp:%08x,%08x,%08x,%08x,sta:%08x\r\n",
    ioreg32_read(BCM2835_EMMC_RESP0),
    ioreg32_read(BCM2835_EMMC_RESP1),
    ioreg32_read(BCM2835_EMMC_RESP2),
    ioreg32_read(BCM2835_EMMC_RESP3),
    ioreg32_read(BCM2835_EMMC_STATUS));

  uint32_t ival = ioreg32_read(BCM2835_EMMC_INTERRUPT);
  printf("data:%08x intr:%08x\r\n", ioreg32_read(BCM2835_EMMC_DATA), ival);

  uint32_t arg = rca << 16;
  // ioreg32_write(BCM2835_EMMC_BLKSIZECNT, 64| (1 << 16));
  ioreg32_write(BCM2835_EMMC_ARG1, arg);

  printf("before cmdtm: rsp:%08x,%08x,%08x,%08x,sta:%08x\r\n",
    ioreg32_read(BCM2835_EMMC_RESP0),
    ioreg32_read(BCM2835_EMMC_RESP1),
    ioreg32_read(BCM2835_EMMC_RESP2),
    ioreg32_read(BCM2835_EMMC_RESP3),
    ioreg32_read(BCM2835_EMMC_STATUS));

  ival = ioreg32_read(BCM2835_EMMC_INTERRUPT);
  printf("data:%08x intr:%08x\r\n", ioreg32_read(BCM2835_EMMC_DATA), ival);

  ioreg32_write(BCM2835_EMMC_CMDTM,
      (9<<24) | /*  CMD9   */
      (0<<21) | /* NO DATA */
      (1<<20) | /* Check response with same idx */
      (1<<19) | /* Check CRC */
      (1<<16) | /* Response type is R2, 136bits */
      (0<<5)  | /* Not a multiblock */
      (1<<4)  | /* Direction of data from card to host */
      (0<<2)  | /* No autocommand */
      (0<<1)    /* No block counting */
  );

  while(!(ioreg32_read(BCM2835_EMMC_STATUS) & (1<<9)))
    printf("Wait bit read ready bit\r\n");

  for (int i = 0 ; i < 20; ++i) {
    ival = ioreg32_read(BCM2835_EMMC_INTERRUPT);
    ioreg32_write(BCM2835_EMMC_INTERRUPT, ival);
    printf("data:%08x,intr:%08x\r\n", ioreg32_read(BCM2835_EMMC_DATA), ival);
    printf("after: rsp:%08x,%08x,%08x,%08x,sta:%08x\r\n",
      ioreg32_read(BCM2835_EMMC_RESP0),
      ioreg32_read(BCM2835_EMMC_RESP1),
      ioreg32_read(BCM2835_EMMC_RESP2),
      ioreg32_read(BCM2835_EMMC_RESP3),
      ioreg32_read(BCM2835_EMMC_STATUS));
  }
  while(1) {}
}
static inline int bcm2835_emmc_process_sd_scr(uint32_t rca, uint64_t scr,
  bool blocking)
{
  int sd_spec_ver_maj;
  int sd_spec_ver_min;

  struct sd_cmd6_response_raw r = { 0 };

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

  if (buswidth4_supported) {
    err = bcm2835_emmc_set_bus_width4(rca, blocking);
    if (err != SUCCESS)
      return err;
  }

#if 0
  custom_cmd6();
#endif

  BCM2835_EMMC_LOG("Running CMD6 to switch to SDR25\r\n");

  err = bcm2835_emmc_cmd6(CMD6_MODE_CHECK,
    CMD6_ARG_ACCESS_MODE_SDR25,
    CMD6_ARG_CMD_SYSTEM_DEFAULT,
    CMD6_ARG_DRIVER_STRENGTH_DEFAULT,
    CMD6_ARG_POWER_LIMIT_DEFAULT,
    r.data,
    blocking);

  if (err != SUCCESS)
    return err;

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

  BCM2835_EMMC_LOG("Switching to SDR25 is supported, switching...\r\n");

  err = bcm2835_emmc_cmd6(CMD6_MODE_SWITCH,
    CMD6_ARG_ACCESS_MODE_SDR25,
    CMD6_ARG_CMD_SYSTEM_DEFAULT,
    CMD6_ARG_DRIVER_STRENGTH_DEFAULT,
    CMD6_ARG_POWER_LIMIT_DEFAULT,
    r.data,
    blocking);

  if (err != SUCCESS)
    return err;

  bcm2835_emmc_set_high_speed();
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

static void bcm2835_emmc_get_mbox_info(void)
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

static int bcm2835_emmc_control1_reset(void)
{
  int i;
  uint32_t control1;

  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL0, 0);
  //ioreg32_write16(BCM2835_EMMC_CONTROL2, 0);
  // volatile uint16_t *r = 0x3f300000;

#if 0
  printf("%08x %08x %016x %08x %08x\r\n",
    *(volatile uint32_t *)(0x3f300040),
    *(volatile uint32_t *)(0x3f300044),
    *(volatile uint64_t *)(0x3f300040),
    *(volatile uint32_t *)(0x3f30003c),
    *(volatile uint16_t *)(0x3f30003e));

  *(volatile uint16_t *)(0x3f30003e) = 0xffff;
  printf("%08x\r\n", *(volatile uint16_t *)(0x3f30003e));

  *(volatile uint32_t *)(0x3f30003c) = 0xffffffff;
  printf("%08x\r\n", *(volatile uint32_t *)(0x3f30003c));
#endif


  /* Disable and re-set clock */
  /* Reset circuit and disable clock */
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

  return SUCCESS;
}

static void bcm2835_emmc_clock_enable(void)
{
  uint32_t control1;
  BCM2835_EMMC_LOG("Enabling SD clock");
  delay_us(2000);
  control1 = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
  BCM2835_EMMC_CONTROL1_CLR_SET_CLK_EN(control1, 1);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL1, control1);
  delay_us(2000);
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

static int bcm2835_emmc_reset_step0(bool blocking)
{
  uint32_t acmd41_resp;
  uint32_t status;
  int err;

  bcm2835_emmc_get_mbox_info();

  err = bcm2835_emmc_control1_reset();
  if (err != SUCCESS)
    return err;

  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL2, 0);

  /* Slow down clock to setup commands */
  err = bcm2835_emmc_set_clock(/*BCM2835_EMMC_CLOCK_HZ_SETUP*/64);
  if (err != SUCCESS) {
    BCM2835_EMMC_ERR("failed to set clock to %d Hz",
      BCM2835_EMMC_CLOCK_HZ_SETUP);
    return err;
  }

  status = bcm2835_emmc_read_reg(BCM2835_EMMC_STATUS);
  BCM2835_EMMC_LOG("STATUS: %08x", status);

  bcm2835_emmc_clock_enable();
  bcm2835_emmc_interrupt_initialize();

  /* CMD0 - GO_IDLE_STATE - reset device to idle state, no response */
  err = bcm2835_emmc_cmd0(blocking);
  BCM2835_EMMC_CHECK_ERR("Failed at CMD0 (GO_TO_IDLE)");

  /* CMD5 is CHECK_SDIO command, it will timeout for non-SDIO devices */
  err = bcm2835_emmc_cmd5(blocking);
  if (err == SUCCESS) {
    BCM2835_EMMC_CRIT("Detected SDIO card. Not supported");
    return ERR_GENERIC;
  } 

  if (err != ERR_TIMEOUT) {
    BCM2835_EMMC_ERR("CMD5 failed with unexpected error %d", err);
    return err;
  }

  /*
   * After failed command (timeout) we should reset command state machine 
   * to a known state.
   */
  err = bcm2835_emmc_reset_cmd(blocking);
  BCM2835_EMMC_CHECK_ERR("failed to reset command after SDIO_CHECK");

  BCM2835_EMMC_LOG("Detected SD card");

  /* CMD8 -SEND_IF_COND - this checks if SD card supports our voltage */
  err = bcm2835_emmc_cmd8(blocking);
  BCM2835_EMMC_CHECK_ERR("CMD8 (SEND_IF_COND) failed");

  /* Host supports HIGH CAPACITY (SDHC/SDXC) */
  #define ACMD41_ARG_HCS  (1<<30)
  /* SDXC power saving - max performance enable */
  #define ACMD41_ARG_XPC  (1<<28)
  #define ACMD41_ARG_S18R (1<<24)
  #define ACMD41_ARG_VDD  0

  err = bcm2835_emmc_acmd41(0, &acmd41_resp, blocking);
  BCM2835_EMMC_CHECK_ERR("ACMD41 (SD_SEND_OP_COND) failed");
  BCM2835_EMMC_LOG("ACMD41: arg:%08x, resp:%08x", 0, acmd41_resp);

  uint32_t acmd41_arg = acmd41_resp | ACMD41_ARG_HCS | ACMD41_ARG_XPC;
  err = bcm2835_emmc_acmd41(acmd41_arg, &acmd41_resp, blocking);
  BCM2835_EMMC_CHECK_ERR("ACMD41 (SD_SEND_OP_COND) failed");

  while (BIT_IS_CLEAR(acmd41_resp, 31)) {
    err = bcm2835_emmc_acmd41(acmd41_arg, &acmd41_resp, blocking);
    BCM2835_EMMC_CHECK_ERR("ACMD41 (SD_SEND_OP_COND) failed");
    BCM2835_EMMC_LOG("ACMD41: arg:%08x, resp:%08x", acmd41_arg, acmd41_resp);
  }

  bool can_switch_to_1v8 = (acmd41_resp >> 24) & 1;
  bool is_uhs2 = (acmd41_resp >> 29) & 1;
  bool is_sdhc = (acmd41_resp >> 30) & 1;
  bool is_busy = (acmd41_resp >> 31) & 1;

  BCM2835_EMMC_LOG("ACMD41: arg:%08x, resp:%08x, 1v8:%d,UHS-II:%d,"
    "SDHC/SDXC:%d,READY:%d",
    acmd41_resp, acmd41_resp, can_switch_to_1v8, is_uhs2, is_sdhc,is_busy);

  return SUCCESS;
}

static int bcm2835_emmc_get_scr(uint32_t rca, bool blocking)
{
  int err;
  char scrbuf[8] = { 0 };
  char scrbuf_le[8] = { 0 };
  uint64_t scr;

  err = bcm2835_emmc_acmd51(rca, scrbuf, sizeof(scrbuf), blocking);
  BCM2835_EMMC_CHECK_ERR("ACMD51 (SEND_SCR) failed");
  for (int i = 0; i < 8; ++i)
    scrbuf_le[i] = scrbuf[7 - i];

  scr = *(const uint64_t *)scrbuf_le;

  BCM2835_EMMC_LOG("bcm2835_emmc_reset: SCR: %016lx", scr);

  err = bcm2835_emmc_process_sd_scr(rca, scr, blocking);
  BCM2835_EMMC_CHECK_ERR("failed at SCR handling step");
  return SUCCESS;
}

int bcm2835_emmc_reset(bool blocking, uint32_t *rca, uint32_t *device_id)
{
  int err;
  uint32_t bcm2835_emmc_status;
  uint32_t bcm2835_emmc_state;

  err = bcm2835_emmc_reset_step0(blocking);
  if (err != SUCCESS)
    return err;

  uint32_t caps0 = bcm2835_emmc_read_reg(BCM2835_EMMC_CAPABILITIES_0);
  uint32_t caps1 = bcm2835_emmc_read_reg(BCM2835_EMMC_CAPABILITIES_1);
  printf("0:%08x, 1:%08x\r\n", caps0, caps1);


  /* Set normal clock */
  err = bcm2835_emmc_set_clock(/* BCM2835_EMMC_CLOCK_HZ_NORMAL */4);
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
  custom_cmd9(*rca);

  err = bcm2835_emmc_cmd13(*rca, &bcm2835_emmc_status, blocking);
  BCM2835_EMMC_CHECK_ERR("failed at CMD13 (SEND_STATUS) step");

  bcm2835_emmc_state = BCM2835_EMMC_CARD_STATUS_GET_CURRENT_STATE(
    bcm2835_emmc_status);

  BCM2835_EMMC_LOG("bcm2835_emmc_reset: status: %08x, curr state: %d(%s)",
    bcm2835_emmc_status, bcm2835_emmc_state,
    bcm2835_emmc_state_to_string(bcm2835_emmc_state));

  bcm2835_emmc_set_block_size(BCM2835_EMMC_BLOCK_SIZE);

  printf("CMD9\r\n");
  err = bcm2835_emmc_cmd9(*rca, blocking);
  printf("CMD9:%d\r\n", err);
  // BCM2835_EMMC_CHECK_ERR("CMD9 (SEND_CSD) failed");
  while(1);
  // printf("CMD58\r\n");
  // bcm2835_emmc_cmd58(*rca, blocking);
  BCM2835_EMMC_CHECK_ERR("CMD58 (READ_OCR) failed");
  while(1);

  err = bcm2835_emmc_get_scr(*rca, blocking);
  if (err != SUCCESS)
    return err;

  BCM2835_EMMC_LOG("bcm2835_emmc_reset: completed successfully");
  err = bcm2835_emmc_set_clock(64);
  return SUCCESS;
}

#include <stringlib.h>
#include <bitops.h>
#include <errcode.h>
#include <mbox.h>
#include <mbox_props.h>
#include "bcm2835_emmc_utils.h"

static inline uint32_t bcm2835_emmc_get_clock_div(uint32_t target_clock)
{
  uint32_t base_clock;

  if (!mbox_get_clock_rate(MBOX_CLOCK_ID_EMMC, &base_clock))
    return -1;

  if (base_clock == 200000000) {
    if (target_clock == BCM2835_EMMC_CLOCK_HZ_SETUP) {
      return 64;
    }
    if (target_clock == BCM2835_EMMC_CLOCK_HZ_NORMAL) {
      return 4;
    }
  }

  return 0;
}

static inline int bcm2835_emmc_wait_clock_stabilized(uint64_t timeout_usec,
  bool blocking)
{
  return bcm2835_emmc_wait_reg_value(
    BCM2835_EMMC_CONTROL1,
    BCM2835_EMMC_CONTROL1_MASK_CLK_STABLE,
    BCM2835_EMMC_CONTROL1_MASK_CLK_STABLE,
    timeout_usec,
    blocking, NULL);
}

int bcm2835_emmc_set_clock(int target_hz, bool blocking)
{
  int err;
  uint32_t control1;
  uint32_t div;

  div = bcm2835_emmc_get_clock_div(target_hz);

  if (div == 0) {
    BCM2835_EMMC_ERR("emmc_set_clock: failed to deduce clock divisor");
    return ERR_GENERIC;
  }

  if (bcm2835_emmc_wait_cmd_dat_ready())
    return ERR_GENERIC;

  BCM2835_EMMC_LOG("emmc_set_clock: status: %08x",
    bcm2835_emmc_read_reg(BCM2835_EMMC_STATUS));

  control1 = bcm2835_emmc_read_reg(BCM2835_EMMC_CONTROL1);
  BCM2835_EMMC_CONTROL1_CLR_SET_DATA_TOUNIT(control1, 0xb);
  BCM2835_EMMC_CONTROL1_CLR_SET_CLK_INTLEN(control1, 1);
  control1 |= div;
  BCM2835_EMMC_LOG("emmc_set_clock: control1: %08x", control1);
  bcm2835_emmc_write_reg(BCM2835_EMMC_CONTROL1, control1);
  delay_us(6);

  err = bcm2835_emmc_wait_clock_stabilized(1000, blocking);
  if (err) {
    BCM2835_EMMC_LOG("emmc_set_clock: failed to stabilize clock, err: %d");
    return err;
  }

  return SUCCESS;
}

const char *bcm2835_emmc_reg_address_to_name(ioreg32_t reg)
{
  if (reg == BCM2835_EMMC_ARG2) return "BCM2835_EMMC_ARG2";
  if (reg == BCM2835_EMMC_BLKSIZECNT) return "BCM2835_EMMC_BLKSIZECNT";
  if (reg == BCM2835_EMMC_ARG1) return "BCM2835_EMMC_ARG1";
  if (reg == BCM2835_EMMC_CMDTM) return "BCM2835_EMMC_CMDTM";
  if (reg == BCM2835_EMMC_RESP0) return "BCM2835_EMMC_RESP0";
  if (reg == BCM2835_EMMC_RESP1) return "BCM2835_EMMC_RESP1";
  if (reg == BCM2835_EMMC_RESP2) return "BCM2835_EMMC_RESP2";
  if (reg == BCM2835_EMMC_RESP3) return "BCM2835_EMMC_RESP3";
  if (reg == BCM2835_EMMC_DATA) return "BCM2835_EMMC_DATA";
  if (reg == BCM2835_EMMC_STATUS) return "BCM2835_EMMC_STATUS";
  if (reg == BCM2835_EMMC_CONTROL0) return "BCM2835_EMMC_CONTROL0";
  if (reg == BCM2835_EMMC_CONTROL1) return "BCM2835_EMMC_CONTROL1";
  if (reg == BCM2835_EMMC_INTERRUPT) return "BCM2835_EMMC_INTERRUPT";
  if (reg == BCM2835_EMMC_IRPT_MASK) return "BCM2835_EMMC_IRPT_MASK";
  if (reg == BCM2835_EMMC_IRPT_EN) return "BCM2835_EMMC_IRPT_EN";
  if (reg == BCM2835_EMMC_CONTROL2) return "BCM2835_EMMC_CONTROL2";
  if (reg == BCM2835_EMMC_CAPABILITIES_0) return "BCM2835_EMMC_CAPABILITIES_0";
  if (reg == BCM2835_EMMC_CAPABILITIES_1) return "BCM2835_EMMC_CAPABILITIES_1";
  if (reg == BCM2835_EMMC_FORCE_IRPT) return "BCM2835_EMMC_FORCE_IRPT";
  if (reg == BCM2835_EMMC_BOOT_TIMEOUT) return "BCM2835_EMMC_BOOT_TIMEOUT";
  if (reg == BCM2835_EMMC_DBG_SEL) return "BCM2835_EMMC_DBG_SEL";
  if (reg == BCM2835_EMMC_EXRDFIFO_CFG) return "BCM2835_EMMC_EXRDFIFO_CFG";
  if (reg == BCM2835_EMMC_EXRDFIFO_EN) return "BCM2835_EMMC_EXRDFIFO_EN";
  if (reg == BCM2835_EMMC_TUNE_STEP) return "BCM2835_EMMC_TUNE_STEP";
  if (reg == BCM2835_EMMC_TUNE_STEPS_STD) return "BCM2835_EMMC_TUNE_STEPS_STD";
  if (reg == BCM2835_EMMC_TUNE_STEPS_DDR) return "BCM2835_EMMC_TUNE_STEPS_DDR";
  if (reg == BCM2835_EMMC_SPI_INT_SPT) return "BCM2835_EMMC_SPI_INT_SPT";
  if (reg == BCM2835_EMMC_SLOTISR_VER) return "BCM2835_EMMC_SLOTISR_VER";
  return "REG_UNKNOWN";
}

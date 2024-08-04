#include <stringlib.h>
#include <bitops.h>
#include <errcode.h>
#include <mbox.h>
#include <mbox_props.h>
#include "bcm2835_emmc_utils.h"

static inline int bcm2835_emmc_wait_clock_stabilized(uint64_t timeout_usec)
{
  return bcm2835_emmc_wait_reg_value(
    BCM2835_EMMC_CONTROL1,
    BCM2835_EMMC_CONTROL1_MASK_CLK_STABLE,
    BCM2835_EMMC_CONTROL1_MASK_CLK_STABLE,
    timeout_usec,
    NULL);
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

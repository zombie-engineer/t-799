#include <stringlib.h>
#include <bitops.h>
#include <errcode.h>
#include "emmc_bcm2835_utils.h"

const char *bcm2835_emmc_reg_address_to_name(ioreg32_t reg)
{
  if (reg == BCM2835_EMMC_ARG2) return "ARG2";
  if (reg == BCM2835_EMMC_BLKSIZECNT) return "BLKSIZECNT";
  if (reg == BCM2835_EMMC_ARG1) return "ARG1";
  if (reg == BCM2835_EMMC_CMDTM) return "CMDTM";
  if (reg == BCM2835_EMMC_RESP0) return "RESP0";
  if (reg == BCM2835_EMMC_RESP1) return "RESP1";
  if (reg == BCM2835_EMMC_RESP2) return "RESP2";
  if (reg == BCM2835_EMMC_RESP3) return "RESP3";
  if (reg == BCM2835_EMMC_DATA) return "DATA";
  if (reg == BCM2835_EMMC_STATUS) return "STATUS";
  if (reg == BCM2835_EMMC_CONTROL0) return "CONTROL0";
  if (reg == BCM2835_EMMC_CONTROL1) return "CONTROL1";
  if (reg == BCM2835_EMMC_INTERRUPT) return "INTERRUPT";
  if (reg == BCM2835_EMMC_IRPT_MASK) return "IRPT_MASK";
  if (reg == BCM2835_EMMC_IRPT_EN) return "IRPT_EN";
  if (reg == BCM2835_EMMC_CONTROL2) return "CONTROL2";
  if (reg == BCM2835_EMMC_CAPABILITIES_0) return "CAPABILITIES_0";
  if (reg == BCM2835_EMMC_CAPABILITIES_1) return "CAPABILITIES_1";
  if (reg == BCM2835_EMMC_FORCE_IRPT) return "FORCE_IRPT";
  if (reg == BCM2835_EMMC_BOOT_TIMEOUT) return "BOOT_TIMEOUT";
  if (reg == BCM2835_EMMC_DBG_SEL) return "DBG_SEL";
  if (reg == BCM2835_EMMC_EXRDFIFO_CFG) return "EXRDFIFO_CFG";
  if (reg == BCM2835_EMMC_EXRDFIFO_EN) return "EXRDFIFO_EN";
  if (reg == BCM2835_EMMC_TUNE_STEP) return "TUNE_STEP";
  if (reg == BCM2835_EMMC_TUNE_STEPS_STD) return "TUNE_STEPS_STD";
  if (reg == BCM2835_EMMC_TUNE_STEPS_DDR) return "TUNE_STEPS_DDR";
  if (reg == BCM2835_EMMC_SPI_INT_SPT) return "SPI_INT_SPT";
  if (reg == BCM2835_EMMC_SLOTISR_VER) return "SLOTISR_VER";
  return "REG_UNKNOWN";
}

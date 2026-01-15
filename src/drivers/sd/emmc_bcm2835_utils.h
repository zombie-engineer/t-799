#pragma once
#include <delay.h>
#include <stdbool.h>
#include <compiler.h>
#include "emmc_bcm2835_log.h"
#include "emmc_bcm2835_regs.h"
#include "emmc_bcm2835_regs_bits.h"
#include <common.h>
#include <config.h>

const char *bcm2835_emmc_reg_address_to_name(ioreg32_t reg);

static inline void OPTIMIZED bcm2835_emmc_write_reg(ioreg32_t reg, uint32_t val)
{
#ifdef CONFIG_BCM2835_SDHC_LOG_REG_IO
  BCM2835_EMMC_LOG("reg write: reg: %08x(%s), value:%08x", reg,
    bcm2835_emmc_reg_address_to_name(reg), val);
#endif
  ioreg32_write(reg, val);
}

static inline uint32_t OPTIMIZED bcm2835_emmc_read_reg(ioreg32_t reg)
{
  uint32_t val;
  val = ioreg32_read(reg);
#ifdef CONFIG_BCM2835_SDHC_LOG_REG_IO
  BCM2835_EMMC_LOG("reg read: reg: %08x(%s), value:%08x", reg,
    bcm2835_emmc_reg_address_to_name(reg), val);
#endif
  return val;
}

/*
 * mask == 0:          Wait until reg value equals any of bits in value
 * mask == 0xffffffff: Wait until reg value equals value
 * mask == any other : Wait until reg value equals (mask & value)
 */
static inline int OPTIMIZED bcm2835_emmc_wait_reg_value(ioreg32_t regaddr,
  uint32_t mask, uint32_t value, uint64_t timeout_usec, uint32_t *val)
{
  const uint64_t wait_step_usec = 100;
  uint64_t wait_left_usec = timeout_usec;
  uint32_t regval;

  while(wait_left_usec >= wait_step_usec) {
    regval = bcm2835_emmc_read_reg(regaddr);
    if (mask) {
      if ((regval & mask) == value)
        goto wait_good;
    } else {
      if (regval & value)
        goto wait_good;
    }
    delay_us(wait_step_usec);
    wait_left_usec -= wait_step_usec;
  }
  return ERR_TIMEOUT;

wait_good:
  if (val)
    *val = regval;
  return SUCCESS;
}

static inline int bcm2835_emmc_interrupt_wait_done_or_err(
  uint64_t timeout_usec, bool waitcmd, bool waitdat, uint32_t *intval)
{
  uint32_t intmask;
  intmask = BCM2835_EMMC_INTERRUPT_MASK_ERR;
  if (waitcmd)
    intmask |= BCM2835_EMMC_INTERRUPT_MASK_CMD_DONE;
  if (waitdat)
    intmask |= BCM2835_EMMC_INTERRUPT_MASK_DATA_DONE;

  return bcm2835_emmc_wait_reg_value(BCM2835_EMMC_INTERRUPT, 0, intmask,
    timeout_usec, intval);
}

static inline int bcm2835_emmc_wait_cmd_inhibit(void)
{
  int i;
  uint32_t status;
  for (i = 0; i < 1000; ++i) {
    status = bcm2835_emmc_read_reg(BCM2835_EMMC_STATUS);
    if (!BCM2835_EMMC_STATUS_GET_CMD_INHIBIT(status))
      break;
    delay_us(1000);
  }
  if (i == 1000)
    return -1;
  return 0;
}

static inline int bcm2835_emmc_wait_dat_inhibit(void)
{
  int i;
  uint32_t status;
  for (i = 0; i < 1000; ++i) {
    status = bcm2835_emmc_read_reg(BCM2835_EMMC_STATUS);
    if (!BCM2835_EMMC_STATUS_GET_DAT_INHIBIT(status))
      break;
    delay_us(1000);
  }
  if (i == 1000)
    return -1;
  return 0;
}

#define BCM2835_EMMC_CLOCK_HZ_SETUP 400000
#define BCM2835_EMMC_CLOCK_HZ_NORMAL 25000000

//int bcm2835_emmc_set_clock(int target_hz);

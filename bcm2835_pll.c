#include <bcm2835/bcm2835_pll.h>
#include "bcm2835_pll_regs.h"
#include <printf.h>
#include <ioreg.h>

static float bcm2835_report_pll(char pll_symbol, ioreg32_t ctrl_reg,
  ioreg32_t frac_reg, ioreg32_t prescale_reg)
{
  uint32_t pll_ctrl, ndiv, pdiv, ndiv_frac_val;
  float ndiv_frac;
  float freq;
  bool should_prescale;
  uint32_t prescale;
  pll_ctrl = ioreg32_read(ctrl_reg);
  ndiv = pll_ctrl & 0x3ff;
  pdiv = (pll_ctrl >> 12) & 7;
  ndiv_frac_val = ioreg32_read(frac_reg) & 0xfffff;
  ndiv_frac = (float)(ndiv_frac_val) / (1<<20);
  prescale = ioreg32_read(prescale_reg);
  if (prescale_reg == A2W_PLLH_ANA1)
    prescale >>= 11;
  else
    prescale >>= 14;

  prescale = (prescale & 1) ? 2 : 1;
  
  freq = 19.2f * prescale * (ndiv + ndiv_frac) / pdiv;
  printf("PLL%c: CTRL:%08x,FRAC:%08x,PRE:%08x\r\n", pll_symbol, pll_ctrl,
    ioreg32_read(frac_reg), ioreg32_read(prescale_reg));

  printf("PLL%c: n=%d, fra:%d, pdiv=%d, pre=%d,freq=%d\r\n", pll_symbol, ndiv,
    ndiv_frac_val, pdiv, prescale, (uint32_t)freq);

  return freq;
}

static float bcm2835_report_pll_child(char pll, const char *name,
  ioreg32_t pll_reg, float parent_freq)
{
  uint32_t ctrl = ioreg32_read(pll_reg);
  uint32_t div = ctrl & 0xff;
  bool ena = (ctrl >> 8) & 1;
  bool bypass = (ctrl >> 9) & 1;

  float freq = div ? (parent_freq  / div) : 0;

  printf("PLL%c chan:'%s',%08x,f:%d,ena:%d,bypass:%d\r\n", pll, name, ctrl,
    (uint32_t)freq, ena, bypass);

  return freq;
}

void bcm2835_report_clocks(void)
{
  float fplla, fpllb, fpllc, fplld, fpllh;

  fplla = bcm2835_report_pll('A', A2W_PLLA_CTRL, A2W_PLLA_FRAC, A2W_PLLA_ANA1);
  fpllb = bcm2835_report_pll('B', A2W_PLLB_CTRL, A2W_PLLB_FRAC, A2W_PLLB_ANA1);
  fpllc = bcm2835_report_pll('C', A2W_PLLC_CTRL, A2W_PLLC_FRAC, A2W_PLLC_ANA1);
  fplld = bcm2835_report_pll('D', A2W_PLLD_CTRL, A2W_PLLD_FRAC, A2W_PLLD_ANA1);
  fpllh = bcm2835_report_pll('H', A2W_PLLH_CTRL, A2W_PLLH_FRAC, A2W_PLLH_ANA1);

  bcm2835_report_pll_child('A', "dsi0", A2W_PLLA_DSI0, fplla);
  bcm2835_report_pll_child('A', "core0", A2W_PLLA_CORE, fplla);
  bcm2835_report_pll_child('A', "per" , A2W_PLLA_PER , fplla);
  bcm2835_report_pll_child('A', "ccp2", A2W_PLLA_CCP2, fplla);

  bcm2835_report_pll_child('B', "arm", A2W_PLLB_ARM, fpllb);
  bcm2835_report_pll_child('B', "sp0", A2W_PLLB_SP0, fpllb);
  bcm2835_report_pll_child('B', "sp1" , A2W_PLLB_SP1 , fpllb);
  bcm2835_report_pll_child('B', "sp2", A2W_PLLB_SP2, fpllb);

  bcm2835_report_pll_child('C', "core2", A2W_PLLC_CORE2, fpllc);
  bcm2835_report_pll_child('C', "core1", A2W_PLLC_CORE1, fpllc);
  float fperc = bcm2835_report_pll_child('C', "per" , A2W_PLLC_PER , fpllc);
  bcm2835_report_pll_child('C', "core0", A2W_PLLC_CORE0, fpllc);

  bcm2835_report_pll_child('D', "dsi0", A2W_PLLD_DSI0, fplld);
  bcm2835_report_pll_child('D', "core", A2W_PLLD_CORE, fplld);
  bcm2835_report_pll_child('D', "per" , A2W_PLLD_PER , fplld);
  bcm2835_report_pll_child('D', "dsi1", A2W_PLLD_DSI1, fplld);

  bcm2835_report_pll_child('H', "aux", A2W_PLLH_AUX, fpllh);
  bcm2835_report_pll_child('H', "rcal", A2W_PLLH_RCAL, fpllh);
  bcm2835_report_pll_child('H', "pix" , A2W_PLLH_PIX , fpllh);
  uint32_t emmc_ctrl = *((volatile uint32_t *)0x3f1011c0);
  uint32_t emmc_div = *((volatile uint32_t *)0x3f1011c4);
  printf("EMMC_CTRL:%08x,DIV:%08x\r\n", emmc_ctrl, emmc_div);
  emmc_div >>= 4;
  float emmc_freq = emmc_div ? fperc / emmc_div : 0;
  printf("EMMC_FREQ:%d\r\n", (uint32_t)emmc_freq);
}

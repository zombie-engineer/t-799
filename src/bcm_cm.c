#include <bcm2835/bcm_cm.h>
#include <ioreg.h>
#include <bitops.h>
#include <common.h>

#define BCM_CM_BASE 0x3f101000

#define CM_PASSWORD		0x5a000000

#define CM_GNRICCTL		0x000
#define CM_GNRICDIV		0x004
# define CM_DIV_FRAC_BITS	12
# define CM_DIV_FRAC_MASK	GENMASK(CM_DIV_FRAC_BITS - 1, 0)

#define CM_UNKNOWN 0xffffffff
#define CM_VPUCTL		0x008
#define CM_VPUDIV		0x00c
#define CM_SYSCTL		0x010
#define CM_SYSDIV		0x014
#define CM_PERIACTL		0x018
#define CM_PERIADIV		0x01c
#define CM_PERIICTL		0x020
#define CM_PERIIDIV		0x024
#define CM_H264CTL		0x028
#define CM_H264DIV		0x02c
#define CM_ISPCTL		0x030
#define CM_ISPDIV		0x034
#define CM_V3DCTL		0x038
#define CM_V3DDIV		0x03c
#define CM_CAM0CTL		0x040
#define CM_CAM0DIV		0x044
#define CM_CAM1CTL		0x048
#define CM_CAM1DIV		0x04c
#define CM_CCP2CTL		0x050
#define CM_CCP2DIV		0x054
#define CM_DSI0ECTL		0x058
#define CM_DSI0EDIV		0x05c
#define CM_DSI0PCTL		0x060
#define CM_DSI0PDIV		0x064
#define CM_DPICTL		0x068
#define CM_DPIDIV		0x06c
#define CM_GP0CTL		0x070
#define CM_GP0DIV		0x074
#define CM_GP1CTL		0x078
#define CM_GP1DIV		0x07c
#define CM_GP2CTL		0x080
#define CM_GP2DIV		0x084
#define CM_HSMCTL		0x088
#define CM_HSMDIV		0x08c
#define CM_OTPCTL		0x090
#define CM_OTPDIV		0x094
#define CM_PCMCTL		0x098
#define CM_PCMDIV		0x09c
#define CM_PWMCTL		0x0a0
#define CM_PWMDIV		0x0a4
#define CM_SLIMCTL		0x0a8
#define CM_SLIMDIV		0x0ac
#define CM_SMICTL		0x0b0
#define CM_SMIDIV		0x0b4
/* no definition for 0x0b8  and 0x0bc */
#define CM_TCNTCTL		0x0c0
# define CM_TCNT_SRC1_SHIFT		12
#define CM_TCNTCNT		0x0c4
#define CM_TECCTL		0x0c8
#define CM_TECDIV		0x0cc
#define CM_TD0CTL		0x0d0
#define CM_TD0DIV		0x0d4
#define CM_TD1CTL		0x0d8
#define CM_TD1DIV		0x0dc
#define CM_TSENSCTL		0x0e0
#define CM_TSENSDIV		0x0e4
#define CM_TIMERCTL		0x0e8
#define CM_TIMERDIV		0x0ec
#define CM_UARTCTL		0x0f0
#define CM_UARTDIV		0x0f4
#define CM_VECCTL		0x0f8
#define CM_VECDIV		0x0fc
#define CM_DSI0HSCK		0x120
#define CM_PULSECTL		0x190
#define CM_PULSEDIV		0x194
#define CM_SDCCTL		0x1a8
#define CM_SDCDIV		0x1ac
#define CM_ARMCTL		0x1b0
#define CM_AVEOCTL		0x1b8
#define CM_AVEODIV		0x1bc
#define CM_EMMCCTL		0x1c0
#define CM_EMMCDIV		0x1c4
#define CM_EMMC2CTL		0x1d0
#define CM_EMMC2DIV		0x1d4

/* General bits for the CM_*CTL regs */
# define CM_ENABLE			BIT(4)
# define CM_KILL			BIT(5)
# define CM_GATE_BIT			6
# define CM_GATE			BIT(CM_GATE_BIT)
# define CM_BUSY			BIT(7)
# define CM_BUSYD			BIT(8)
# define CM_FRAC			BIT(9)
# define CM_SRC_SHIFT			0
# define CM_SRC_BITS			4
# define CM_SRC_MASK			0xf
# define CM_SRC_GND			0
# define CM_SRC_OSC			1
# define CM_SRC_TESTDEBUG0		2
# define CM_SRC_TESTDEBUG1		3
# define CM_SRC_PLLA_CORE		4
# define CM_SRC_PLLA_PER		4
# define CM_SRC_PLLC_CORE0		5
# define CM_SRC_PLLC_PER		5
# define CM_SRC_PLLC_CORE1		8
# define CM_SRC_PLLD_CORE		6
# define CM_SRC_PLLD_PER		6
# define CM_SRC_PLLH_AUX		7
# define CM_SRC_PLLC_CORE1		8
# define CM_SRC_PLLC_CORE2		9

#define CM_OSCCOUNT		0x100

#define CM_PLLA			0x104
# define CM_PLL_ANARST			BIT(8)
# define CM_PLLA_HOLDPER		BIT(7)
# define CM_PLLA_LOADPER		BIT(6)
# define CM_PLLA_HOLDCORE		BIT(5)
# define CM_PLLA_LOADCORE		BIT(4)
# define CM_PLLA_HOLDCCP2		BIT(3)
# define CM_PLLA_LOADCCP2		BIT(2)
# define CM_PLLA_HOLDDSI0		BIT(1)
# define CM_PLLA_LOADDSI0		BIT(0)

#define CM_PLLC			0x108
# define CM_PLLC_HOLDPER		BIT(7)
# define CM_PLLC_LOADPER		BIT(6)
# define CM_PLLC_HOLDCORE2		BIT(5)
# define CM_PLLC_LOADCORE2		BIT(4)
# define CM_PLLC_HOLDCORE1		BIT(3)
# define CM_PLLC_LOADCORE1		BIT(2)
# define CM_PLLC_HOLDCORE0		BIT(1)
# define CM_PLLC_LOADCORE0		BIT(0)

#define CM_PLLD			0x10c
# define CM_PLLD_HOLDPER		BIT(7)
# define CM_PLLD_LOADPER		BIT(6)
# define CM_PLLD_HOLDCORE		BIT(5)
# define CM_PLLD_LOADCORE		BIT(4)
# define CM_PLLD_HOLDDSI1		BIT(3)
# define CM_PLLD_LOADDSI1		BIT(2)
# define CM_PLLD_HOLDDSI0		BIT(1)
# define CM_PLLD_LOADDSI0		BIT(0)

#define CM_PLLH			0x110
# define CM_PLLH_LOADRCAL		BIT(2)
# define CM_PLLH_LOADAUX		BIT(1)
# define CM_PLLH_LOADPIX		BIT(0)

#define CM_LOCK			0x114
# define CM_LOCK_FLOCKH			BIT(12)
# define CM_LOCK_FLOCKD			BIT(11)
# define CM_LOCK_FLOCKC			BIT(10)
# define CM_LOCK_FLOCKB			BIT(9)
# define CM_LOCK_FLOCKA			BIT(8)

#define CM_EVENT		0x118
#define CM_DSI1ECTL		0x158
#define CM_DSI1EDIV		0x15c
#define CM_DSI1PCTL		0x160
#define CM_DSI1PDIV		0x164
#define CM_DFTCTL		0x168
#define CM_DFTDIV		0x16c

#define CM_PLLB			0x170
# define CM_PLLB_HOLDARM		BIT(1)
# define CM_PLLB_LOADARM		BIT(0)

#define A2W_PLLA_CTRL		0x1100
#define A2W_PLLC_CTRL		0x1120
#define A2W_PLLD_CTRL		0x1140
#define A2W_PLLH_CTRL		0x1160
#define A2W_PLLB_CTRL		0x11e0
# define A2W_PLL_CTRL_PRST_DISABLE	BIT(17)
# define A2W_PLL_CTRL_PWRDN		BIT(16)
# define A2W_PLL_CTRL_PDIV_MASK		0x000007000
# define A2W_PLL_CTRL_PDIV_SHIFT	12
# define A2W_PLL_CTRL_NDIV_MASK		0x0000003ff
# define A2W_PLL_CTRL_NDIV_SHIFT	0

#define A2W_PLLA_ANA0		0x1010
#define A2W_PLLC_ANA0		0x1030
#define A2W_PLLD_ANA0		0x1050
#define A2W_PLLH_ANA0		0x1070
#define A2W_PLLB_ANA0		0x10f0

#define A2W_PLL_KA_SHIFT	7
#define A2W_PLL_KA_MASK		GENMASK(9, 7)
#define A2W_PLL_KI_SHIFT	19
#define A2W_PLL_KI_MASK		GENMASK(21, 19)
#define A2W_PLL_KP_SHIFT	15
#define A2W_PLL_KP_MASK		GENMASK(18, 15)

#define A2W_PLLH_KA_SHIFT	19
#define A2W_PLLH_KA_MASK	GENMASK(21, 19)
#define A2W_PLLH_KI_LOW_SHIFT	22
#define A2W_PLLH_KI_LOW_MASK	GENMASK(23, 22)
#define A2W_PLLH_KI_HIGH_SHIFT	0
#define A2W_PLLH_KI_HIGH_MASK	GENMASK(0, 0)
#define A2W_PLLH_KP_SHIFT	1
#define A2W_PLLH_KP_MASK	GENMASK(4, 1)

#define A2W_XOSC_CTRL		0x1190
# define A2W_XOSC_CTRL_PLLB_ENABLE	BIT(7)
# define A2W_XOSC_CTRL_PLLA_ENABLE	BIT(6)
# define A2W_XOSC_CTRL_PLLD_ENABLE	BIT(5)
# define A2W_XOSC_CTRL_DDR_ENABLE	BIT(4)
# define A2W_XOSC_CTRL_CPR1_ENABLE	BIT(3)
# define A2W_XOSC_CTRL_USB_ENABLE	BIT(2)
# define A2W_XOSC_CTRL_HDMI_ENABLE	BIT(1)
# define A2W_XOSC_CTRL_PLLC_ENABLE	BIT(0)

#define A2W_PLLA_FRAC		0x1200
#define A2W_PLLC_FRAC		0x1220
#define A2W_PLLD_FRAC		0x1240
#define A2W_PLLH_FRAC		0x1260
#define A2W_PLLB_FRAC		0x12e0
# define A2W_PLL_FRAC_MASK		((1 << A2W_PLL_FRAC_BITS) - 1)
# define A2W_PLL_FRAC_BITS		20

#define A2W_PLL_CHANNEL_DISABLE		BIT(8)
#define A2W_PLL_DIV_BITS		8
#define A2W_PLL_DIV_SHIFT		0

#define A2W_PLLA_DSI0		0x1300
#define A2W_PLLA_CORE		0x1400
#define A2W_PLLA_PER		0x1500
#define A2W_PLLA_CCP2		0x1600

#define A2W_PLLC_CORE2		0x1320
#define A2W_PLLC_CORE1		0x1420
#define A2W_PLLC_PER		0x1520
#define A2W_PLLC_CORE0		0x1620

#define A2W_PLLD_DSI0		0x1340
#define A2W_PLLD_CORE		0x1440
#define A2W_PLLD_PER		0x1540
#define A2W_PLLD_DSI1		0x1640

#define A2W_PLLH_AUX		0x1360
#define A2W_PLLH_RCAL		0x1460
#define A2W_PLLH_PIX		0x1560
#define A2W_PLLH_STS		0x1660

#define A2W_PLLH_CTRLR		0x1960
#define A2W_PLLH_FRACR		0x1a60
#define A2W_PLLH_AUXR		0x1b60
#define A2W_PLLH_RCALR		0x1c60
#define A2W_PLLH_PIXR		0x1d60
#define A2W_PLLH_STSR		0x1e60

#define A2W_PLLB_ARM		0x13e0
#define A2W_PLLB_SP0		0x14e0
#define A2W_PLLB_SP1		0x15e0
#define A2W_PLLB_SP2		0x16e0

#define LOCK_TIMEOUT_NS		100000000
#define BCM2835_MAX_FB_RATE	1750000000u

#define SOC_BCM2835		BIT(0)
#define SOC_BCM2711		BIT(1)
#define SOC_ALL			(SOC_BCM2835 | SOC_BCM2711)

#define VCMSG_ID_CORE_CLOCK     4

static const int bcm2835_clock_vpu_parents[] = {
	BCM2835_CLOCK_GND,
	BCM2835_CLOCK_OSC,
	BCM2835_CLOCK_TESTDEBUG0,
	BCM2835_CLOCK_TESTDEBUG1,
	BCM2835_PLLA_CORE,
	BCM2835_PLLC_CORE0,
	BCM2835_PLLD_CORE,
	BCM2835_PLLH_AUX,
	BCM2835_PLLC_CORE1,
	BCM2835_PLLC_CORE2
};

static const int bcm_cm_clock_id_to_offsets[] = {
  CM_PLLA,
  CM_PLLB,
  CM_PLLC,
  CM_PLLD,
  CM_PLLH,
  A2W_PLLA_CORE,
  A2W_PLLA_DSI0,
  A2W_PLLB_ARM,
  A2W_PLLC_CORE0,
  A2W_PLLC_CORE1,
  A2W_PLLC_CORE2,
  A2W_PLLC_PER,
  A2W_PLLD_CORE,
  A2W_PLLD_PER,
  A2W_PLLH_RCAL,
  A2W_PLLH_AUX,
  A2W_PLLH_PIX,
  CM_TIMERCTL,
  CM_OTPCTL,
  CM_UARTCTL,
  CM_VPUCTL,
  CM_UNKNOWN /* BCM2835_CLOCK_V3D */,
  CM_ISPCTL,
  CM_H264CTL,
  CM_UNKNOWN /* BCM2835_CLOCK_VEC */,
  CM_HSMCTL,
  CM_SDCCTL,
  CM_TSENSCTL,
  CM_EMMCCTL,
  CM_PERIICTL,
  CM_PWMCTL,
  CM_PCMCTL,
  A2W_PLLA_DSI0,
  A2W_PLLA_CCP2,
  A2W_PLLD_DSI0,
  A2W_PLLD_DSI1,
  CM_AVEOCTL,
  CM_DFTCTL,
  CM_GP0CTL,
  CM_GP1CTL,
  CM_GP2CTL,
  CM_SLIMCTL,
  CM_SMICTL,
  CM_TECCTL,
  CM_DPICTL,
  CM_CAM0CTL,
  CM_CAM1CTL,
  CM_DSI0ECTL,
  CM_DSI1ECTL,
  CM_DSI0PCTL,
  CM_DSI1PCTL,
  CM_EMMC2CTL,
  BCM2835_CLOCK_GND,
  BCM2835_CLOCK_OSC,
  BCM2835_CLOCK_TESTDEBUG0,
  BCM2835_CLOCK_TESTDEBUG1,
  BCM2835_CLOCK_UNKNOWN,
  BCM2835_CLOCK_NONE,
};

static const char *bcm2835_clock_names[] = {
#define CLOCK_NAME(__a) #__a
  CLOCK_NAME(PLLA),
  CLOCK_NAME(PLLB),
  CLOCK_NAME(PLLC),
  CLOCK_NAME(PLLD),
  CLOCK_NAME(PLLH),
  CLOCK_NAME(PLLA_CORE),
  CLOCK_NAME(PLLA_PER),
  CLOCK_NAME(PLLB_ARM),
  CLOCK_NAME(PLLC_CORE0),
  CLOCK_NAME(PLLC_CORE1),
  CLOCK_NAME(PLLC_CORE2),
  CLOCK_NAME(PLLC_PER),
  CLOCK_NAME(PLLD_CORE),
  CLOCK_NAME(PLLD_PER),
  CLOCK_NAME(PLLH_RCAL),
  CLOCK_NAME(PLLH_AUX),
  CLOCK_NAME(PLLH_PIX),
  CLOCK_NAME(CLOCK_TIMER),
  CLOCK_NAME(CLOCK_OTP),
  CLOCK_NAME(CLOCK_UART),
  CLOCK_NAME(CLOCK_VPU),
  CLOCK_NAME(CLOCK_V3D),
  CLOCK_NAME(CLOCK_ISP),
  CLOCK_NAME(CLOCK_H264),
  CLOCK_NAME(CLOCK_VEC),
  CLOCK_NAME(CLOCK_HSM),
  CLOCK_NAME(CLOCK_SDRAM),
  CLOCK_NAME(CLOCK_TSENS),
  CLOCK_NAME(CLOCK_EMMC),
  CLOCK_NAME(CLOCK_PERI_IMAGE),
  CLOCK_NAME(CLOCK_PWM),
  CLOCK_NAME(CLOCK_PCM),
  CLOCK_NAME(PLLA_DSI0),
  CLOCK_NAME(PLLA_CCP2),
  CLOCK_NAME(PLLD_DSI0),
  CLOCK_NAME(PLLD_DSI1),
  CLOCK_NAME(CLOCK_AVEO),
  CLOCK_NAME(CLOCK_DFT),
  CLOCK_NAME(CLOCK_GP0),
  CLOCK_NAME(CLOCK_GP1),
  CLOCK_NAME(CLOCK_GP2),
  CLOCK_NAME(CLOCK_SLIM),
  CLOCK_NAME(CLOCK_SMI),
  CLOCK_NAME(CLOCK_TEC),
  CLOCK_NAME(CLOCK_DPI),
  CLOCK_NAME(CLOCK_CAM0),
  CLOCK_NAME(CLOCK_CAM1),
  CLOCK_NAME(CLOCK_DSI0E),
  CLOCK_NAME(CLOCK_DSI1E),
  CLOCK_NAME(CLOCK_DSI0P),
  CLOCK_NAME(CLOCK_DSI1P),
  CLOCK_NAME(BCM2711_CLOCK_EMMC2),
  CLOCK_NAME(CLOCK_GND),
  CLOCK_NAME(CLOCK_OSC),
  CLOCK_NAME(CLOCK_TESTDEBUG0),
  CLOCK_NAME(CLOCK_TESTDEBUG1),
  CLOCK_NAME(CLOCK_UNKNOWN),
  CLOCK_NAME(CLOCK_NONE)
#undef CLOCK_NAME
};

uint32_t bcm_cm_read_reg_ctl_vpu(void)
{
  return ioreg32_read((ioreg32_t)(BCM_CM_BASE + CM_VPUCTL));
}

uint32_t bcm_cm_read_reg_div_vpu(void)
{
  return ioreg32_read((ioreg32_t)(BCM_CM_BASE + CM_VPUDIV));
}

static __attribute__((unused)) float bcm_cm_get_pll_child_info(char pll, const char *name,
  ioreg32_t pll_reg, float parent_freq)
{
  uint32_t ctrl = ioreg32_read(pll_reg);
  uint32_t div = ctrl & 0xff;
  bool __attribute__((unused)) ena = (ctrl >> 8) & 1;
  bool __attribute__((unused)) bypass = (ctrl >> 9) & 1;

  float freq = div ? (parent_freq  / div) : 0;

#if 0
  printf("PLL%c chan:'%s',%08x,f:%d,ena:%d,bypass:%d\r\n", pll, name, ctrl,
    (uint32_t)freq, ena, bypass);
#endif

  return freq;
}

bool bcm_cm_get_clock_info(int clock_id, bool *enabled, int *div,
  int *parent_clock_id, const char **parent_clock_name)
{
  ioreg32_t reg_ctl;
  ioreg32_t reg_div;
  int parent_clk_idx;
  uint32_t r;
  uint64_t regaddr;
  int parent_clk = 0;
  bool local_enabled;

  if (clock_id >= ARRAY_SIZE(bcm_cm_clock_id_to_offsets))
    return false;

  if (bcm_cm_clock_id_to_offsets[clock_id] == CM_UNKNOWN)
    return false;

  regaddr = BCM_CM_BASE + bcm_cm_clock_id_to_offsets[clock_id];
  reg_ctl = (ioreg32_t)regaddr;
  reg_div = reg_ctl + 1;
  r = ioreg32_read(reg_ctl);
  parent_clk_idx = (r >> CM_SRC_SHIFT) & CM_SRC_MASK;
  local_enabled = r & CM_ENABLE;

  if (clock_id == BCM2835_CLOCK_VPU) {
    /* VPU has no enabled bit, always on */
    local_enabled = true;
    if (parent_clk_idx >= ARRAY_SIZE(bcm2835_clock_vpu_parents))
      return false;

    parent_clk = bcm2835_clock_vpu_parents[parent_clk_idx];
  }

  *parent_clock_name = bcm2835_clock_names[parent_clk];
  *parent_clock_id = parent_clk;
  *div = ioreg32_read(reg_div);
  *enabled = local_enabled;
  return true;
}

#include <bcm2835/bcm2835_ic.h>
#include <memory_map.h>
#include <ioreg.h>

#define BCM2835_IC_BASE   (uint64_t)(BCM2835_MEM_PERIPH_BASE + 0xb200)
#define BCM2835_IC_PENDING_BASIC (ioreg32_t)(BCM2835_IC_BASE + 0x00)
#define BCM2835_IC_PENDING_IRQ_1 (ioreg32_t)(BCM2835_IC_BASE + 0x04)
#define BCM2835_IC_PENDING_IRQ_2 (ioreg32_t)(BCM2835_IC_BASE + 0x08)
#define BCM2835_IC_FIQ_CONTROL   (ioreg32_t)(BCM2835_IC_BASE + 0x0c)
#define BCM2835_IC_ENABLE_IRQ_1  (ioreg32_t)(BCM2835_IC_BASE + 0x10)
#define BCM2835_IC_ENABLE_IRQ_2  (ioreg32_t)(BCM2835_IC_BASE + 0x14)
#define BCM2835_IC_ENABLE_BASIC  (ioreg32_t)(BCM2835_IC_BASE + 0x18)
#define BCM2835_IC_DISABLE_IRQ_1 (ioreg32_t)(BCM2835_IC_BASE + 0x1c)
#define BCM2835_IC_DISABLE_IRQ_2 (ioreg32_t)(BCM2835_IC_BASE + 0x20)
#define BCM2835_IC_DISABLE_BASIC (ioreg32_t)(BCM2835_IC_BASE + 0x24)

void bcm2835_ic_write_irq_reg(int irqnr, ioreg32_t basereg)
{
  uint32_t regval;

  ioreg32_t reg = basereg;
  if (irqnr > 32) {
    irqnr %= 32;
    reg++;
  }
  
  regval = ioreg32_read(reg);
  regval |= 1 << irqnr;
  ioreg32_write(reg, regval);
}

void bcm2835_ic_enable_irq(int irqnr)
{
  bcm2835_ic_write_irq_reg(irqnr, BCM2835_IC_ENABLE_IRQ_1);
}

void bcm2835_ic_disable_irq(int irqnr)
{
  bcm2835_ic_write_irq_reg(irqnr, BCM2835_IC_DISABLE_IRQ_1);
}

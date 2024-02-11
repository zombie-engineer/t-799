#pragma once

#define GPU_IRQ(__nr) __nr
#define ARM_IRQ(__nr) (__nr + 64)
#define IS_GPU_IRQ(__nr) (__nr < 64)

#define BCM2835_IRQNR_SYSTIMER_0 GPU_IRQ(0)
#define BCM2835_IRQNR_SYSTIMER_1 GPU_IRQ(1)
#define BCM2835_IRQNR_SYSTIMER_2 GPU_IRQ(2)
#define BCM2835_IRQNR_SYSTIMER_3 GPU_IRQ(3)
#define BCM2835_IRQNR_USB        GPU_IRQ(9)
#define BCM2835_IRQNR_AUX        GPU_IRQ(29)
#define BCM2835_IRQNR_UART0      GPU_IRQ(57)
#define BCM2835_IRQNR_MAX        GPU_IRQ(63)

#define BCM2835_IRQNR_ARM_TIMER             ARM_IRQ(0)
#define BCM2835_IRQNR_ARM_MAILBOX           ARM_IRQ(1)
#define BCM2835_IRQNR_ARM_DOORBELL_0        ARM_IRQ(2)
#define BCM2835_IRQNR_ARM_DOORBELL_1        ARM_IRQ(3)
#define BCM2835_IRQNR_ARM_GPU_0_HALTED      ARM_IRQ(4)
#define BCM2835_IRQNR_ARM_GPU_1_HALTED      ARM_IRQ(5)
#define BCM2835_IRQNR_ARM_ACCESS_ERR_TYPE_0 ARM_IRQ(6)
#define BCM2835_IRQNR_ARM_ACCESS_ERR_TYPE_1 ARM_IRQ(7)

#define BCM2835_IRQNR_ARM_MAX               INTR_CTL_IRQ_ARM_ACCESS_ERR_TYPE_1

void bcm2835_ic_enable_irq(int irqnr);
void bcm2835_ic_disable_irq(int irqnr);

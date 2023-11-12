#pragma once

#define BCM2835_IRQNR_SYSTIMER_0 0
#define BCM2835_IRQNR_SYSTIMER_1 1
#define BCM2835_IRQNR_SYSTIMER_2 2
#define BCM2835_IRQNR_SYSTIMER_3 3

void bcm2835_ic_enable_irq(int irqnr);

#pragma once

#define NUM_IRQS 128

/*
 *  __irq_routine - a hint notice, that indicates the function is bound
 *  to a particular IRQ number.
 */
#define __irq_routine

typedef void(*irq_func)(void);

void __handle_irq(int irqnr);

void irq_init(void);

int irq_set(int irqnr, irq_func func);

int irq_local_set(irq_func func);

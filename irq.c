#include <irq.h>
#include <string.h>
#include <errcode.h>
#include <common.h>

struct irq_desc {
  irq_func handler;
};

static struct irq_desc irq_table[NUM_IRQS];
static struct irq_desc irq_local;

int irq_set(int irqnr, irq_func func)
{
  struct irq_desc *idesc;

  if (irqnr >= NUM_IRQS)
    return ERR_INVAL;

  idesc = &irq_table[irqnr];
  if (idesc->handler)
    return ERR_BUSY;

  idesc->handler = func;

  return SUCCESS;
}

void __handle_irq(int irqnr)
{
  struct irq_desc *idesc;

  if (irqnr > NUM_IRQS)
    panic();

  idesc = &irq_table[irqnr];

  if (idesc->handler)
    idesc->handler();
}

int irq_local_set(irq_func func)
{
  irq_local.handler = func;
  return SUCCESS;
}

void irq_init(void)
{
  memset(irq_table, 0, sizeof(irq_table));
  irq_local.handler = 0;
}

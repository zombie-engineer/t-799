#include <irq.h>
#include <string.h>
#include <errcode.h>
#include <common.h>
#include <sections.h>

struct irq_desc {
  irq_func handler;
};

static BSS_NOMMU struct irq_desc irq_table[NUM_IRQS];
static BSS_NOMMU struct irq_desc irq_local;

int irq_set(int irqnr, irq_func func)
{
  struct irq_desc *idesc;
  struct irq_desc *table;

  if (irqnr >= NUM_IRQS)
    return ERR_INVAL;

  asm volatile ("ldr %0, =irq_table\n" :"=r"(table));
  idesc = &table[irqnr];
  if (idesc->handler)
    return ERR_BUSY;

  idesc->handler = func;

  return SUCCESS;
}

EXCEPTION void __handle_irq(int irqnr)
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
  struct irq_desc *local;

  asm volatile ("ldr %0, =irq_table\n" :"=r"(local));
  local->handler = func;
  return SUCCESS;
}

void irq_init(void)
{
  struct irq_desc *table;
  struct irq_desc *local;

  asm volatile ("ldr %0, =irq_table\n" :"=r"(table));
  asm volatile ("ldr %0, =irq_local\n" :"=r"(local));
  memset(table, 0, sizeof(irq_table));
  local->handler = 0;
}

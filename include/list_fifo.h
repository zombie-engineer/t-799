#include <list.h>
#include <cpu.h>

static inline struct list_head *list_fifo_pop_head(struct list_head *l)
{
  int irqflags;
  struct list_head *result;

  disable_irq_save_flags(irqflags);

  if (list_empty(l)) {
    restore_irq_flags(irqflags);
    return NULL;
  }
  
  result = l->next;
  list_del(result);
  restore_irq_flags(irqflags);
  return result;
}

static inline void list_fifo_push_tail_locked(struct list_head *l,
  struct list_head *entry)
{
  list_add_tail(entry, l);
}

static inline void list_fifo_push_tail(struct list_head *l,
  struct list_head *entry)
{
  int irqflags;
  disable_irq_save_flags(irqflags);
  list_fifo_push_tail_locked(l, entry);
  restore_irq_flags(irqflags);
}

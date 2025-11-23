#include <semaphore.h>
#include <common.h>
#include <sched.h>
#include <event.h>
#include <os_api.h>

#if 0
static inline void __semaphore_down(struct semaphore *s)
{
  waiter.waiter_task = sched_get_current_task();
  os_event_clear(&waiter.ev);
  spinlock_unlock_irq(&s->lock);
  os_event_wait(&waiter.ev);
  spinlock_lock_irq(&s->lock);
}

static inline void __semaphore_up(struct semaphore *s)
{
  struct semaphore_waiter *waiter;

  waiter = list_first_entry(&s->wait_list, struct semaphore_waiter, list);
  os_event_notify(&waiter->ev);
}
#endif

void semaphore_give(struct semaphore *s)
{
#if 0
  // int flag;

  // spinlock_lock_disable_irq(&s->lock, flag);

  if (s->value)
    __semaphore_down(s);

  s->count--;
  spinlock_unlock_restore_irq(&s->lock, flag);
#endif
}

void semaphore_take(struct semaphore *s)
{
#if 0
  int flag;
  spinlock_lock_disable_irq(&s->lock, flag);
  if (list_empty(&s->wait_list))
    s->count++;
  else
    __semaphore_up(s);
  spinlock_unlock_restore_irq(&s->lock, flag);
#endif
}

#if 0
int down_trylock(struct semaphore *s)
{
  int flags;
  uint64_t count;

  spinlock_lock_disable_irq(&s->lock, flags);
  count = s->count - 1;
  if (count >= 0)
    s->count = count;
  spinlock_unlock_restore_irq(&s->lock, flags);
  return count < 0;
}
#endif

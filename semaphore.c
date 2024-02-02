#include <semaphore.h>
#include <common.h>
#include <sched.h>
#include <event.h>
#include <os_api.h>

struct semaphore_waiter {
  struct list_head list;
  struct task *waiter_task;
  struct event ev;
};

static inline void __semaphore_down(struct semaphore *s)
{
  struct semaphore_waiter waiter;

  list_add_tail(&waiter.list, &s->wait_list);
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


void semaphore_down(struct semaphore *s)
{
  int flag;
  spinlock_lock_disable_irq(&s->lock, flag);
  if (s->count > 0)
    s->count--;
  else
    __semaphore_down(s);
  spinlock_unlock_restore_irq(&s->lock, flag);
}

void semaphore_up(struct semaphore *s)
{
  int flag;
  spinlock_lock_disable_irq(&s->lock, flag);
  if (list_empty(&s->wait_list))
    s->count++;
  else
    __semaphore_up(s);
  spinlock_unlock_restore_irq(&s->lock, flag);
}

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

#include <sched.h>
#include <task.h>
#include <cpu.h>
#include <stdint.h>
#include <common.h>
#include <bcm2835/bcm2835_systimer.h>
#include "armv8_cpuctx.h"
#include <printf.h>
#if 0
#define SCHED_PRINTF(__fmt, ...) printf(__fmt, ##__VA_ARGS__)
#else
#define SCHED_PRINTF(__fmt, ...) ;
#endif

#define SCHED_MS_PER_TICK 10
#define MS_TO_US(__ms) (__ms * 1000)
#define MS_TO_TICKS(__ms) ((__ms) / (SCHED_MS_PER_TICK))

extern struct armv8_cpuctx *__current_cpuctx;
extern struct armv8_cpuctx __cpuctx_stub;

static void sched_idle_task_fn(void)
{
  while(1)
    asm volatile("wfe");
}

struct scheduler {
  struct task *current;
  struct list_head runnable;
  struct list_head blocked_on_event;
  struct list_head blocked_on_timer;
  struct timer *sched_timer;
  int timer_interval_ms;
  struct task *idle_task;
  uint64_t ticks;
};

static struct scheduler sched;

struct task *sched_get_current_task(void)
{
  return sched.current;
}

bool sched_run_task_isr(struct task *t)
{
  struct list_head *node;
  struct task *old_task;

  uint32_t s = offsetof(struct task, scheduler_list);
  if (sched.current == t)
    return false;

  list_for_each(node, &sched.runnable) {
    old_task = container_of(node, struct task, scheduler_list);
    if (old_task == t)
      return false;
  }

  list_for_each(node, &sched.blocked_on_event) {
    old_task = container_of(node, struct task, scheduler_list);
    if (old_task == t)
      return false;
  }

  list_add_tail(&t->scheduler_list, &sched.runnable);
  return true;
}

static inline void scheduler_insert_to_blocked_on_timer(struct task *t)
{
  struct list_head *head = &sched.blocked_on_timer;
  struct list_head *node;

  if (head->next == head) {
    head->next = head->prev = &t->scheduler_list;
    t->scheduler_list.next = t->scheduler_list.prev = head;
    return;
  }

  node = head->next;
  while (node != head) {
    struct task *old_t = container_of(node, struct task, scheduler_list);
    if (old_t->next_wakeup_time > t->next_wakeup_time)
      break;
    node = node->next;
  }

  t->scheduler_list.prev = node->prev;
  t->scheduler_list.prev->next = &t->scheduler_list;
  t->scheduler_list.next = node;
  node->prev = &t->scheduler_list;
#if 0
  list_for_each_entry(t, head, scheduler_list)
    printf("timer_list: %p, %s, %ld\n", t, t->name, t->next_wakeup_time);
#endif
}

static inline void scheduler_drop_current(void)
{
  struct task *t;

  t = sched.current;
  sched.current = NULL;

  if (t == sched.idle_task)
    return;

  if (t->scheduler_request == TASK_SCHED_RQ_EXIT) {
    task_delete_isr(t);
    return;
  }
  else if (t->scheduler_request == TASK_SCHED_RQ_BLOCK_ON_TIMER)
    scheduler_insert_to_blocked_on_timer(t);
  else if (t->scheduler_request == TASK_SCHED_RQ_BLOCK_ON_EVENT)
    list_add_tail(&t->scheduler_list, &sched.blocked_on_event);
  else
    list_add_tail(&t->scheduler_list, &sched.runnable);
  t->scheduler_request = 0;
}

static void scheduler_select_next(void)
{
  struct task *t;
  struct list_head *h;

  if (!list_empty(&sched.blocked_on_event)) {
    h = sched.blocked_on_event.next;
    t = container_of(h, struct task, scheduler_list);
    BUG_IF(!t->wait_event, "wait_event should not be NULL");
    if (t->wait_event->ev == 1) {
      list_del(h);
      sched.current = t;
      goto done;
    }
  }

  if (!list_empty(&sched.blocked_on_timer)) {
    h = sched.blocked_on_timer.next;
    t = container_of(h, struct task, scheduler_list);
    if (sched.ticks >= t->next_wakeup_time) {
      list_del(h);
      sched.current = t;
      goto done;
    }
  }

  if (!list_empty(&sched.runnable)) {
    h = sched.runnable.next;
    list_del(h);
    t = container_of(h, struct task, scheduler_list);
    sched.current = t;
    goto done;
  }

  sched.current = sched.idle_task;

done:
  asm volatile (
    "ldr x1, =__current_cpuctx\n"
    "str %0, [x1]\n"::"r"(sched.current->cpuctx));
}

static void __schedule(void)
{
  SCHED_PRINTF("__schedule, task:%s\r\n", sched.current->name);
  scheduler_drop_current();
  scheduler_select_next();
  SCHED_PRINTF("__schedule end, new task:%s\r\n", sched.current->name);
}

static void sched_timer_irq_cb(void *arg)
{
  sched.ticks++;
  __schedule();
  bcm2835_systimer_start_oneshot(
    MS_TO_US(SCHED_MS_PER_TICK),
    sched_timer_irq_cb, NULL);
}

void NORETURN scheduler_start(void)
{
  irq_disable();
  sched.current = sched.idle_task;
  asm volatile (
    "ldr x1, =__current_cpuctx\n"
    "str %0, [x1]\n"::"r"(sched.current->cpuctx));

  bcm2835_systimer_start_oneshot(MS_TO_US(SCHED_MS_PER_TICK),
    sched_timer_irq_cb, NULL);
  irq_enable();

  while(1)
    asm volatile("wfe");
}

bool sched_exit_current_task_isr(void)
{
  struct task *t = sched.current;
  t->scheduler_request = TASK_SCHED_RQ_EXIT;
  __schedule();
}

bool sched_exit_task_isr(struct task *t)
{
  if (t == sched.current) {
    t->scheduler_request = TASK_SCHED_RQ_EXIT;
    __schedule();
  }
  else {
    list_del(&t->scheduler_list);
    task_delete_isr(t);
  }
}


void sched_delay_current_ms_isr(uint64_t ms)
{
  struct task *t = sched.current;
  t->scheduler_request = TASK_SCHED_RQ_BLOCK_ON_TIMER;
  t->next_wakeup_time = sched.ticks + MS_TO_TICKS(ms);
  __schedule();
}

void sched_event_wait_isr(struct event *ev)
{
  struct task *t = sched.current;
  t->scheduler_request = TASK_SCHED_RQ_BLOCK_ON_EVENT;
  t->wait_event = ev;
  __schedule();
}

void sched_event_notify(struct event *ev)
{
  int irqflags;
  struct list_head *node;
  struct list_head *tmp;

  struct list_head tmp_head = LIST_HEAD_INIT(tmp_head);
  struct task *t;

  disable_irq_save_flags(irqflags);
  ev->ev = 1;

  list_for_each_safe(node, tmp, &sched.blocked_on_event) {
    t = container_of(node, struct task, scheduler_list);
    if (t->wait_event == ev)
      list_move(node, &tmp_head);
  }

  list_for_each_safe(node, tmp, &tmp_head)
    list_move(node, &sched.blocked_on_event);

  restore_irq_flags(irqflags);
}

void scheduler_init(void)
{
  INIT_LIST_HEAD(&sched.runnable);
  INIT_LIST_HEAD(&sched.blocked_on_event);
  INIT_LIST_HEAD(&sched.blocked_on_timer);
  sched.current = NULL;
  sched.ticks = 0;

  /* __current_cpuctx should be initilized before first call to task_create */
  sched.idle_task = task_create(sched_idle_task_fn, "idle");
  BUG_IF(!sched.idle_task, "Faild to add idle task");
}

void scheduler_yield_isr(void)
{
  __schedule();
}

uint64_t sched_get_time_us(void)
{
  uint64_t result = 0;
  int flags;
  disable_irq_save_flags(flags);
  // result = MS_TO_US(sched.ticks * SCHED_MS_PER_TICK);
  result =+ bcm2835_systimer_get_time_us_locked();
  restore_irq_flags(flags);
  return result;
}

#include <sched.h>
#include <task.h>
#include <cpu.h>
#include <stdint.h>
#include <common.h>
#include <bcm2835/bcm2835_systimer.h>
#include "armv8_cpuctx.h"

#define SCHED_MS_PER_TICK 100
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

bool scheduler_start_task(struct task *t)
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
}

static inline void scheduler_drop_current(void)
{
	struct task *t;

	t = sched.current;
	sched.current = NULL;

	if (t == sched.idle_task)
		return;

	if (t->scheduler_request == TASK_SCHED_RQ_BLOCK_ON_TIMER)
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
	__current_cpuctx = sched.current->cpuctx;
}

static void __schedule(void)
{
	scheduler_drop_current();
	scheduler_select_next();
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
	scheduler_select_next();
	__current_cpuctx = &__cpuctx_stub;
	bcm2835_systimer_start_oneshot(MS_TO_US(SCHED_MS_PER_TICK), sched_timer_irq_cb, NULL);
	irq_enable();
	while(1)
		asm volatile("wfe");
}

void scheduler_delay_current_ms(uint64_t ms)
{
	struct task *t = sched.current;
	t->scheduler_request = TASK_SCHED_RQ_BLOCK_ON_TIMER;
	t->next_wakeup_time = sched.ticks + MS_TO_TICKS(ms);
	__schedule();
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
	BUG_IF(!sched.idle_task);
}

void scheduler_yield(void)
{
	__schedule();
}

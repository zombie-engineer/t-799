#include <sched.h>
#include <task.h>
#include <cpu.h>
#include <debug_led.h>
#include <stdint.h>
#include <common.h>
#include <bcm2835/bcm2835_systimer.h>
#include "armv8_cpuctx.h"

extern struct armv8_cpuctx *__current_cpuctx;
extern struct armv8_cpuctx __cpuctx_stub;

#define SCHED_TIME_SLICE_MS 60000

volatile int global_timer = 0;
volatile int global_timer2 = 0;

static void sched_idle_task_fn(void)
{
  while(1) {
    asm volatile("wfe");
  }
}

struct scheduler {
	struct task *current;
	struct list_head runnable;
	struct list_head blocked;
	struct timer *sched_timer;
	int timer_interval_ms;
	struct task *idle_task;
};

static struct scheduler sched;

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

	list_for_each(node, &sched.blocked) {
		old_task = container_of(node, struct task, scheduler_list);
		if (old_task == t)
			return false;
	}

	list_add_tail(&t->scheduler_list, &sched.runnable);
	return true;
}

static inline void scheduler_drop_current(void)
{
  struct task *t;

  t = sched.current;
  sched.current = NULL;

  if (t == sched.idle_task)
    return;

  if (t->sched_attrs & TASK_SCHED_ATTR_RUNNABLE)
    list_add_tail(&t->scheduler_list, &sched.runnable);
  else
    list_add_tail(&t->scheduler_list, &sched.blocked);
}

static void scheduler_select_next(void)
{
  struct task *t;
  struct list_head *h;

  if (!list_empty(&sched.runnable)) {
    h = sched.runnable.next;
    list_del(h);
    t = container_of(h, struct task, scheduler_list);
    sched.current = t;
  }
  else
    sched.current = sched.idle_task;

  __current_cpuctx = (void *)&t->cpuctx;
}

static void scheduler_do_job(void)
{
  scheduler_drop_current();
  scheduler_select_next();
}

static void sched_timer_irq_cb(void *arg)
{
	global_timer++;
	debug_led_toggle();
	scheduler_do_job();
	bcm2835_systimer_start_oneshot(1000, sched_timer_irq_cb, NULL);
}

void NORETURN scheduler_start(void)
{
	irq_disable();
	scheduler_select_next();
	__current_cpuctx = &__cpuctx_stub;
	bcm2835_systimer_start_oneshot(SCHED_TIME_SLICE_MS, sched_timer_irq_cb, NULL);
	irq_enable();
	while(1)
		asm volatile("wfe");
}

void scheduler_init(void)
{
	INIT_LIST_HEAD(&sched.runnable);
	INIT_LIST_HEAD(&sched.blocked);
	sched.current = NULL;
	sched.idle_task = task_create(sched_idle_task_fn);
	debug_led_init();
}

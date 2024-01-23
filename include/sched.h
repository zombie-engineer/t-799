#pragma once
#include <list.h>
#include <stdbool.h>
#include <stdint.h>

struct task;
struct event;

void scheduler_init(void);
void scheduler_start(void);
bool scheduler_start_task(struct task *t);

struct task *sched_get_current_task(void);

/*
 * scheduler_yield stops running current task and schedules next task in the
 * runlist.
 * Should be called from ISR
 */
void scheduler_yield_isr(void);

/*
 * scheduler_delay_current_ms_isr - puts current task to sleep and schedules
 * next task in the runlist.
 * Should be called from ISR
 */
void scheduler_delay_current_ms_isr(uint64_t ms);

void sched_event_wait_isr(struct event *ev);

void sched_event_notify(struct event *ev);

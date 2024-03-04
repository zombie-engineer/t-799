#pragma once
#include <stdint.h>
#include <event.h>

/*
 * os_wait_ms - put current task to sleep for a given number of milliseconds
 * ms - number of milliseconds to sleep
 */
void os_wait_ms(uint32_t ms);

/*
 * os_yield - stop executing current task and give up rest of the time slice
 * for the management of scheduler
 */
void os_yield(void);

void os_event_init(struct event *ev);
void os_event_clear(struct event *ev);
void os_event_wait(struct event *ev);

struct task;

void os_schedule_task(struct task *t);

void os_exit_task(struct task *t);

void os_exit_current_task(void);

void os_event_notify(struct event *ev);

void svc_handler(uint32_t imm);

#pragma once
#include <stdint.h>
#include <event.h>
#include <cpu.h>
#include <syscall.h>

#define os_api_syscall1(__syscall, __arg0) \
  do { \
    asm inline volatile( \
        "mov x0, %0\r\n" \
        "svc %1" \
    :: "r"(__arg0), "i"(__syscall)); \
  } while (0)

#define os_api_syscall2(__syscall, __arg0, __arg1) \
  do { \
    asm inline volatile( \
      "mov x0, %0\r\n" \
      "mov x1, %1\r\n" \
      "svc %2" \
    :: "r"(__arg0), "r"(__arg1), "i"(__syscall)); \
  } while (0)

#define os_api_put_to_wait_list(__wait_list) \
  do { \
    asm inline volatile("mov x0, %0\r\nsvc %1" \
    :: "r"(__wait_list), "i"(SYSCALL_PUT_TO_WAIT_LIST)); \
  } while (0)

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
void os_event_notify_isr(struct event *ev);
void os_event_notify_and_yield(struct event *ev);

void svc_handler(uint32_t imm);

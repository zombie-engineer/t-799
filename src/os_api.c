#include <os_api.h>
#include <sched.h>
#include <task.h>
#include <os_msgq.h>
#include "armv8_cpuctx.h"
#include <common.h>
#include <errcode.h>

#if 0
#include <printf.h>
#define OSAPI_PUTS(__msg) puts(__msg)
#else
#define OSAPI_PUTS(__msg) ;
#endif

void os_wait_ms(uint32_t ms)
{
  asm inline volatile("mov x0, %0\r\nsvc %1"
    :: "r"(ms), "i"(SYSCALL_WAIT));
}

void os_yield(void)
{
  asm inline volatile("svc %0" :: "i"(SYSCALL_YIELD));
}

void os_event_init(struct event *ev)
{
  ev->ev = 0;
}

void os_event_clear(struct event *ev)
{
  OSAPI_PUTS("[os_event_clear]\r\n");
  ev->ev = 0;
  OSAPI_PUTS("[os_event_clear end]\r\n");
}

void os_schedule_task(struct task *t)
{
  asm inline volatile("mov x0, %0\r\nsvc %1"
    :: "r"(t), "i"(SYSCALL_SCHEDULE_TASK));
}

void os_exit_task(struct task *t)
{
  asm inline volatile("mov x0, %0\r\nsvc %1"
    :: "r"(t), "i"(SYSCALL_EXIT_TASK));
}

void os_exit_current_task(void)
{
  asm inline volatile("svc %0" :: "i"(SYSCALL_EXIT_CURRENT_TASK));
}

void os_event_wait(struct event *ev)
{
  int irq;
  OSAPI_PUTS("[os_event_wait]\r\n");
  disable_irq_save_flags(irq);
  if (ev->ev == 1) {
    OSAPI_PUTS("[os_event_wait end fast]\r\n");
    goto out;
  }

  asm inline volatile("mov x0, %0\r\nsvc %1"
    :: "r"(ev), "i"(SYSCALL_WAIT_EVENT));
  OSAPI_PUTS("[os_event_wait end slow]\r\n");
out:
  restore_irq_flags(irq);
}

void os_event_notify_isr(struct event *ev)
{
  OSAPI_PUTS("[os_event_notify_isr]\r\n");
  sched_event_notify_isr(ev);
  OSAPI_PUTS("[os_event_notify_isr end]\r\n");
}

void os_event_notify(struct event *ev)
{
  OSAPI_PUTS("[os_event_notify]\r\n");
  sched_event_notify(ev);
  OSAPI_PUTS("[os_event_notify end]\r\n");
}

void os_event_notify_and_yield(struct event *ev)
{
  int irq;

  OSAPI_PUTS("[os_event_notify]\r\n");
  disable_irq_save_flags(irq);
  sched_event_notify(ev);
  os_yield();
  restore_irq_flags(irq);
  OSAPI_PUTS("[os_event_notify end]\r\n");
}

static inline void svc_sema_give(struct semaphore *s)
{
  os_semaphore_give_isr(s);
}

static inline void svc_put_to_wait_list(struct list_head *l)
{
  sched_wait_list_put_current_isr(l);
}

static inline void svc_msgq_put(struct os_msgq *q, const void *m)
{
  os_msgq_put_isr(q, m);
}

void svc_handler(uint32_t imm)
{
  uint64_t arg0, arg1;
  struct task *t = sched_get_current_task();
  const struct armv8_cpuctx *c;
  c = t->cpuctx;
  arg0 = c->gpregs[0];
  arg1 = c->gpregs[1];

  switch(imm)
  {
  case SYSCALL_WAIT:
    sched_delay_current_ms_isr(arg0);
    break;
  case SYSCALL_YIELD:
    scheduler_yield_isr();
    break;
  case SYSCALL_WAIT_EVENT:
    sched_event_wait_isr((struct event *)arg0);
    break;
  case SYSCALL_SCHEDULE_TASK:
    sched_run_task_isr((struct task *)arg0);
    break;
  case SYSCALL_EXIT_TASK:
    sched_exit_task_isr((struct task *)arg0);
    break;
  case SYSCALL_EXIT_CURRENT_TASK:
    sched_exit_current_task_isr();
    break;
  case SYSCALL_SEMA_GIVE:
    svc_sema_give((void *)arg0);
    break;
  case SYSCALL_PUT_TO_WAIT_LIST:
    svc_put_to_wait_list((void *)arg0);
    break;
  case SYSCALL_MSGQ_PUT:
    svc_msgq_put((void *)arg0, (const void *)arg1);
    break;
  default:
    panic();
    break;
  }
}

#include <os_api.h>
#include <sched.h>
#include <task.h>
#include "armv8_cpuctx.h"
#include <common.h>

#if 0
#include <printf.h>
#define OSAPI_PUTS(__msg) puts(__msg)
#else
#define OSAPI_PUTS(__msg) ;
#endif
#define SVC_WAIT 0
#define SVC_YIELD 1
#define SVC_WAIT_EVENT 2
#define SVC_SCHEDULE_TASK 3
#define SVC_EXIT_TASK 4
#define SVC_EXIT_CURRENT_TASK 5

void os_wait_ms(uint32_t ms)
{
  asm inline volatile("mov x0, %0\r\nsvc %1"
    :: "r"(ms), "i"(SVC_WAIT));
}

void os_yield(void)
{
  asm inline volatile("svc %0" :: "i"(SVC_YIELD));
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
    :: "r"(t), "i"(SVC_SCHEDULE_TASK));
}

void os_exit_task(struct task *t)
{
  asm inline volatile("mov x0, %0\r\nsvc %1"
    :: "r"(t), "i"(SVC_EXIT_TASK));
}

void os_exit_current_task(void)
{
  asm inline volatile("svc %0" :: "i"(SVC_EXIT_CURRENT_TASK));
}

void os_event_wait(struct event *ev)
{
  OSAPI_PUTS("[os_event_wait]\r\n");
  if (ev->ev == 1) {
    OSAPI_PUTS("[os_event_wait end fast]\r\n");
    return;
  }

  asm inline volatile("mov x0, %0\r\nsvc %1"
    :: "r"(ev), "i"(SVC_WAIT_EVENT));
  OSAPI_PUTS("[os_event_wait end slow]\r\n");
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


void svc_handler(uint32_t imm)
{
  uint64_t arg0;
  struct task *t = sched_get_current_task();
  const struct armv8_cpuctx *c;
  c = t->cpuctx;
  arg0 = c->gpregs[0];

  switch(imm)
  {
  case SVC_WAIT:
    sched_delay_current_ms_isr(arg0);
    break;
  case SVC_YIELD:
    scheduler_yield_isr();
    break;
  case SVC_WAIT_EVENT:
    sched_event_wait_isr((struct event *)arg0);
    break;
  case SVC_SCHEDULE_TASK:
    sched_run_task_isr((struct task *)arg0);
    break;
  case SVC_EXIT_TASK:
    sched_exit_task_isr((struct task *)arg0);
    break;
  case SVC_EXIT_CURRENT_TASK:
    sched_exit_current_task_isr();
    break;
  default:
    panic();
    break;
  }
}

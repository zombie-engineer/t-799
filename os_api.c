#include <os_api.h>
#include <sched.h>
#include <task.h>
#include "armv8_cpuctx.h"
#include <common.h>

#define SVC_WAIT 0
#define SVC_YIELD 1

void os_wait_ms(uint32_t ms)
{
  asm inline volatile("mov x0, %0\r\nsvc %1"
    :: "r"(ms), "i"(SVC_WAIT));
}

void os_yield(void)
{
  asm inline volatile("svc %0" :: "i"(SVC_YIELD));
}

static inline void os_api_handle_svc_wait(uint64_t arg)
{
  scheduler_delay_current_ms(arg);
}

static inline void os_api_handle_svc_yield(void)
{
  scheduler_yield();
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
    os_api_handle_svc_wait(arg0);
    break;
  case SVC_YIELD:
    os_api_handle_svc_yield();
    break;
  default:
    panic();
    break;
  }
}

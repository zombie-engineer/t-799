#include <os_api.h>
#include <sched.h>
#include <task.h>
#include "armv8_cpuctx.h"

#define SVC_WAIT 0

void os_wait_ms(uint32_t ms)
{
  asm inline volatile("mov x0, %0\r\nsvc %1":: "r"(ms), "i"(SVC_WAIT));
}

static inline void os_api_handle_svc_wait(uint64_t arg)
{
  scheduler_delay_current_ms(arg);
}

void svc_handler(uint32_t imm)
{
  uint64_t arg0;
  struct task *t = sched_get_current_task();
  const struct armv8_cpuctx *c;
  c = t->cpuctx;
  arg0 = c->gpregs[0];

  if (imm == SVC_WAIT)
  {
    os_api_handle_svc_wait(arg0);
  }
}

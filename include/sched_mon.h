#pragma once
#include <config.h>
#include <stdint.h>

typedef enum {
  SCHED_MON_EV_NONE = 0,
  SCHED_MON_EV_SAVE_CTX,
  SCHED_MON_EV_RESTORE_CTX,
  SCHED_MON_EV_DROP_CURRENT_TASK,
  SCHED_MON_EV_SET_CURRENT_TASK,
  SCHED_MON_EV_IRQ_HANDLER_START,
  SCHED_MON_EV_IRQ_HANDLER_END,
  SCHED_MON_EV_NOT_YET = 0xff,
} sched_mon_event_t;

#if defined(CONFIG_SCHED_MON)
void sched_mon_start(void);
void sched_mon_stop(void);
void sched_mon_event(sched_mon_event_t ev, int value);
#else
static inline void sched_mon_start(void) {}
static inline void inline sched_mon_stop(const char *tag, uint64_t ts0,
  uint64_t ts1) {}
static inline void sched_mon_event(sched_mon_event_t ev, int value) {}
#endif

/* Save context is in assembly file, so inlining not possible */
void sched_mon_save_ctx(void);

static inline void sched_mon_restore_ctx(void)
{
  sched_mon_event(SCHED_MON_EV_RESTORE_CTX, 0);
}

static inline void sched_mon_drop_current_task(int task_id)
{
  sched_mon_event(SCHED_MON_EV_DROP_CURRENT_TASK, task_id);
}

static inline void sched_mon_set_current_task(int task_id)
{
  sched_mon_event(SCHED_MON_EV_SET_CURRENT_TASK, task_id);
}

static inline void sched_mon_set_irq_handler_start(int irq)
{
  sched_mon_event(SCHED_MON_EV_IRQ_HANDLER_START, irq);
}

static inline void sched_mon_set_irq_handler_end(int irq)
{
  sched_mon_event(SCHED_MON_EV_IRQ_HANDLER_END, irq);
}

#pragma once
#include <config.h>
#include <stdint.h>
#if defined(CONFIG_SCHED_MON)
void sched_mon_start(void);
void sched_mon_stop(const char *tag, uint64_t ts0, uint64_t ts1);
void sched_mon_schedule(int task_id);

#else
static inline void sched_mon_start(void) {}
static inline void inline sched_mon_stop(const char *tag, uint64_t ts0,
  uint64_t ts1) {}
static inline void sched_mon_schedule(int task_id) {}
#endif

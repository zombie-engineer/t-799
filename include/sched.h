#pragma once
#include <list.h>

struct scheduler {
  struct list_head running;
  struct list_head timer_waiting;
  struct list_head flag_waiting;
  struct timer *sched_timer;
  int timer_interval_ms;
};

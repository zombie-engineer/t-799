#pragma once
#include <list.h>
#include <stdint.h>

typedef void (*task_fn)(void);

#define TASK_SCHED_RQ_NONE 0
#define TASK_SCHED_RQ_BLOCK_ON_TIMER (1<<0)
#define TASK_SCHED_RQ_BLOCK_ON_EVENT (1<<1)

struct task {
  struct list_head scheduler_list;
  char name[8];
  uint32_t task_id;
  void *cpuctx;
  uint64_t next_wakeup_time;
  uint8_t scheduler_request;
};

struct task *task_create(task_fn fn, const char *task_name);
void mem_allocator_init(void);

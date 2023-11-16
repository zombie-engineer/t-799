#pragma once
#include <list.h>
#include <stdint.h>

typedef void (*task_fn)(void);

#define TASK_SCHED_ATTR_RUNNABLE (1<<0)

struct task {
  struct list_head scheduler_list;
  uint32_t task_id;
  uint32_t sched_attrs;
  char cpuctx[8 * 40];
};

struct task *task_create(task_fn fn);
void mem_allocator_init(void);

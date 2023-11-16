#include <task.h>
#include <common.h>

static uint32_t task_busymask;

struct task tasks_array[32];

static struct task *task_alloc(void)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(tasks_array); ++i) {
    if (task_busymask & (1<<i) == 0) {
      task_busymask |= (1<<i);
      return &tasks_array[i];
    }
  }
  return NULL;
}

struct task *task_create(task_fn fn)
{
  struct task *t;
  t = task_alloc();
  return t;
}

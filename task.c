#include <task.h>
#include <common.h>
#include <string.h>

static uint32_t tasks_busymask = 0;

#define NUM_STACK_WORDS 256
struct stack {
  uint64_t data[NUM_STACK_WORDS];
};

static uint32_t stacks_busymask = 0;

struct stack stacks_array[32];
struct task tasks_array[32];

static struct stack *stack_alloc(void)
{
  int i;
  uint32_t bitmap = stacks_busymask;

  for (i = 0; i < ARRAY_SIZE(stacks_array); ++i) {
    if (!(bitmap & 1)) {
      stacks_busymask |= (1<<i);
      return &stacks_array[i];
    }
    bitmap >>= 1;
  }

  return NULL;
}

static void stack_release(struct stack *s)
{
  int stack_idx = s - &stacks_array[0];
  stacks_busymask &= ~(1<<stack_idx);
}

static struct task *task_alloc(void)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(tasks_array); ++i) {
    if ((tasks_busymask & (1<<i)) == 0) {
      tasks_busymask |= (1<<i);
      return &tasks_array[i];
    }
  }
  return NULL;
}

static void task_return_to_nowhere(void)
{
  panic();
}

void task_init_cpuctx(struct task *t, task_fn fn, uint64_t stack_base)
{
  extern void __armv8_cpuctx_init(uint64_t *cpuctx,
    uint64_t program_counter,
    uint64_t stack_pointer,
    uint64_t link_register);

  __armv8_cpuctx_init((uint64_t *)t->cpuctx, (uint64_t)fn, stack_base,
    (uint64_t)task_return_to_nowhere);
}

struct task *task_create(task_fn fn)
{
  struct stack *s;
  struct task *t;
  uint64_t stack_base;

  /*
   * Stack pointer will be assigned this address and be able to store
   * value at this address. Stack pointer will then "grow upwards", same as
   * decrement its value towards smaller
   */
  s = stack_alloc();
  if (!s)
    return NULL;

  t = task_alloc();
  if (!t) {
    stack_release(s);
    return NULL;
  }

  stack_base = (uint64_t)&s->data[NUM_STACK_WORDS - 1];
  task_init_cpuctx(t, fn, stack_base);
  return t;
}

void mem_allocator_init(void)
{
  tasks_busymask = 0;
  stacks_busymask = 0;
}

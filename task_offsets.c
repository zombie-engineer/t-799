#include <task.h>

struct task_offsets {
  uint8_t task_size;
  uint8_t task_offset_name;
  uint8_t task_offset_id;
  uint8_t task_offset_ctx;
};

const struct task_offsets offsets __attribute__((section(".task_struct_layout")))  = {
  .task_size = sizeof(struct task),
  .task_offset_name = offsetof(struct task, name),
  .task_offset_id = offsetof(struct task, task_id),
  .task_offset_ctx= offsetof(struct task, cpuctx),
};

#!/bin/bash
set -e

CROSS=/home/user_user/gcc-arm-10.3-2021.07-x86_64-aarch64-none-elf/bin/aarch64-none-elf
current_task=0x$($CROSS-objdump kernel8.elf -x  | grep -w sched | grep ff | cut -d' ' -f1)
tasks_array=0x$($CROSS-objdump kernel8.elf -x  | grep -w tasks_array | grep ff | cut -d' ' -f1)
echo '{
  "task_current" : "'$current_task'",
  "tasks_array" : "'$tasks_array'",
  "task_size" : 80,
  "task_offset_name" : 16,
  "task_offset_id" : 32,
  "task_offset_cpuctx" : 40
}'

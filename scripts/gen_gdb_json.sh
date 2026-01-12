#!/bin/bash
set -e
TASK_OFFSETS_BIN=build/task_offsets.bin

read8() {
    local FILENAME=$1
    local OFFSET=$2
    od -An -t u1 -j $OFFSET -N 1 $FILENAME | tr -d ' '
}

CROSS=/home/user_user/gcc-arm-10.3-2021.07-x86_64-aarch64-none-elf/bin/aarch64-none-elf
current_task=0x$($CROSS-objdump build/kernel8.elf -x  | grep -w sched | grep ff | cut -d' ' -f1)
tasks_array=0x$($CROSS-objdump build/kernel8.elf -x  | grep -w tasks_array | grep ff | cut -d' ' -f1)
task_size=$(read8 $TASK_OFFSETS_BIN 0)
task_offset_name=$(read8 $TASK_OFFSETS_BIN 1)
task_offset_id=$(read8 $TASK_OFFSETS_BIN 2)
task_offset_ctx=$(read8 $TASK_OFFSETS_BIN 3)
echo '{
  "task_current" : "'$current_task'",
  "tasks_array" : "'$tasks_array'",
  "task_size" : '$task_size',
  "task_offset_name" : '$task_offset_name',
  "task_offset_id" : '$task_offset_id',
  "task_offset_cpuctx" : '$task_offset_ctx'
}'

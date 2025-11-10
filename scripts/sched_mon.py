#!/usr/bin/env python3
import sys

# Configurable vertical levels
LEVEL_TASK = 1.0
LEVEL_SCHED = 1.5
LEVEL_IRQ = 2.0
LEVEL_IDLE = 0.0
DELTA_US = 7  # microseconds to extend for visual transitions

LEVEL_NO_TASK = 0.0

# Mapping event codes to state transitions
EV_SAVE_CTX = 1
EV_RESTORE_CTX = 2
EV_DROP_TASK = 3
EV_SET_TASK = 4
EV_IRQ_START = 5
EV_IRQ_END = 6

def ev2str(e):
  if e == EV_SAVE_CTX:
    return 'SAVE_CTX'
  if e == EV_RESTORE_CTX:
    return 'RESTORE_CTX'
  if e == EV_DROP_TASK:
    return 'DROP_TASK'
  if e == EV_SET_TASK:
    return 'SET_TASK'
  if e == EV_IRQ_START:
    return 'IRQ_START'
  if e == EV_IRQ_END:
    return 'IRQ_END'
  return 'UNKONWN'

def parse_line(line):
    t, ev, val = map(int, line.strip().split())
    return t, ev, val

def event_to_y(ev):
    if ev == EV_SAVE_CTX:
        return LEVEL_SCHED + 0.05
    if ev == EV_SET_TASK:
        return LEVEL_SCHED + 0.075
    if ev == EV_IRQ_END:
        return LEVEL_SCHED + 0.025
    if ev == EV_DROP_TASK:
        return LEVEL_SCHED

    if ev in [EV_RESTORE_CTX]:
        return LEVEL_TASK

    if ev in [EV_IRQ_START]:
        return LEVEL_IRQ

    raise Exception(f'Unknown event value {ev}')


def file_to_records(input_file):
    records = []
    with open(input_file, 'r') as f:
        for line in f:
            if not line.strip():
                continue
            t, ev, val = parse_line(line)
            records.append((t, ev, val))
    return records


def records_to_data(records):
    t0 = records[0][0]
    level = LEVEL_IDLE
    timeline = []
    task_ids = []
    current_task = None

    def push(time, y, ev):
        print(time - t0, ev2str(ev), y)
        timeline.append((time - t0, y))

    def push_task(time, y, ev, task_id):
        print(f'task: {task_id}', time - t0, ev2str(ev), y)
        task_ids.append((time - t0, y))

    last_y = None
    last_task_id_y = None
    for t, ev, val in records:
        # maintain previous state until now
        y = event_to_y(ev)
        if last_y:
            push(t - DELTA_US, last_y, ev)
        push(t, y, ev)
        last_y = y

        if ev in [EV_DROP_TASK, EV_SET_TASK]:
            if ev == EV_DROP_TASK:
                y = LEVEL_NO_TASK
            else:
                y = LEVEL_NO_TASK + (1 + val) * 0.25
            if last_task_id_y is not None:
              push_task(t - 1, last_task_id_y, ev, val)
            push_task(t, y, ev, val)
            last_task_id_y = y

    return timeline, task_ids


def main(input_file):
    dat_cpu_state = 'data_cpu_state.dat'
    dat_task_ids = 'data_task_ids.dat'
    records = file_to_records(input_file)
    if not records:
        print("No data found.")
        return

    data, task_ids = records_to_data(records)
    # Write final state line
    with open(dat_cpu_state, "w") as f:
        for t, y in data:
            f.write(f"{t} {y}\n")
    print(f"✅ Wrote timeline to {dat_cpu_state}")

    with open(dat_task_ids, "w") as f:
        for t, y in task_ids:
            f.write(f"{t} {y}\n")
    print(f"✅ Wrote timeline to {dat_task_ids}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 sched_timeline.py <input_log>")
        sys.exit(1)
    main(sys.argv[1])


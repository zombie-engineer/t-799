#pragma once
#include <list.h>
#include <stdbool.h>

struct task;

void scheduler_init(void);
void scheduler_start(void);
bool scheduler_start_task(struct task *t);

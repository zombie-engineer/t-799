#pragma once
#include <spinlock.h>
#include <list.h>

struct mutex {
  struct spinlock lock;
  int value;
  struct list_head wait_list;
};

void mutex_init(struct mutex *m);

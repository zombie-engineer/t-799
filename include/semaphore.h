#pragma once
#include <list.h>

struct semaphore {
  int value;
  int max_value;
  struct list_head wait_list;
};

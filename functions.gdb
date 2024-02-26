define lt
  printf "Listing all tasks:\n"
  set $__t = sched.current
  if $__t
    printf "Current: %p, %s\n", $__t, $__t->name
  else
    printf "Current: NONE\n"
  end
  printf "Runnables:\n"
  set $__list_head = &sched.runnable
  set $__list_node = $__list_head->next
  while $__list_node != $__list_head
    set $__t = (struct task *)($__list_node)
    set $__list_node = $__list_node->next
    printf "  %p, %s\n", $__t, $__t->name
  end
  printf "---\n"

  printf "Event waiting:\n"
  set $__list_head = &sched.blocked_on_event
  set $__list_node = $__list_head->next
  while $__list_node != $__list_head
    set $__t = (struct task *)($__list_node)
    set $__ev = $__t->wait_event
    set $__list_node = $__list_node->next
    printf "  %p, %s -> ev:%p\n", $__t, $__t->name, $__ev
  end
  printf "---\n"

  printf "Time waiting:\n"
  set $__list_head = &sched.blocked_on_timer
  set $__list_node = $__list_head->next
  while $__list_node != $__list_head
    set $__t = (struct task *)($__list_node)
    set $__list_node = $__list_node->next
    printf "  %p, %s\n", $__t, $__t->name
  end
  printf "---\n"
end

define reboot_cpu
  printf "---REBOOTING\n"
  p $pc = 0x80000
  p *(uint32_t *)0x3f100024 = (0x5a << 24) | 1
  p *(uint32_t *)0x3f10001c = (0x5a << 24) | 0x20
  monitor reset init
end

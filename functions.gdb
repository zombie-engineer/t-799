define __print_irq_basic
  if $arg0 & (1<<0)
    printf "ARM_TIMER,"
  end
  if $arg0 & (1<<1)
    printf "ARM_MAILBOX,"
  end
  if $arg0 & (1<<2)
    printf "ARM_DOORBELL_0,"
  end
  if $arg0 & (1<<3)
    printf "ARM_DOORBELL_1,"
  end
  if $arg0 & (1<<4)
    printf "GPU0_HALTED,"
  end
  if $arg0 & (1<<5)
    printf "GPU1_HALTED,"
  end
  if $arg0 & (1<<6)
    printf "ILLEGAL_ACCESS_0,"
  end
  if $arg0 & (1<<7)
    printf "ILLEGAL_ACCESS_1,"
  end
end

define __print_irq_1
  if $arg0 & (1<<0)
    printf "SYSTIMER_0,"
  end
  if $arg0 & (1<<1)
    printf "SYSTIMER_1,"
  end
  if $arg0 & (1<<2)
    printf "SYSTIMER_2,"
  end
  if $arg0 & (1<<3)
    printf "SYSTIMER_3,"
  end
  if $arg0 & (1<<16)
    printf "DMA_CH_0,"
  end
  if $arg0 & (1<<17)
    printf "DMA_CH_1,"
  end
  if $arg0 & (1<<18)
    printf "DMA_CH_2,"
  end
  if $arg0 & (1<<19)
    printf "DMA_CH_3,"
  end
  if $arg0 & (1<<20)
    printf "DMA_CH_4,"
  end
  if $arg0 & (1<<21)
    printf "DMA_CH_5,"
  end
  if $arg0 & (1<<22)
    printf "DMA_CH_6,"
  end
  if $arg0 & (1<<23)
    printf "DMA_CH_7,"
  end
  if $arg0 & (1<<24)
    printf "DMA_CH_8,"
  end
  if $arg0 & (1<<25)
    printf "DMA_CH_9,"
  end
  if $arg0 & (1<<26)
    printf "DMA_CH_10,"
  end
  if $arg0 & (1<<27)
    printf "DMA_CH_11,"
  end
  if $arg0 & (1<<28)
    printf "DMA_CH_12,"
  end
  if $arg0 & (1<<29)
    printf "AUX_INT,"
  end
  if $arg0 & (1<<43)
    printf "I2C_SPI_SLV_INT,"
  end
  if $arg0 & (1<<45)
    printf "PWA0,"
  end
  if $arg0 & (1<<46)
    printf "PWA0,"
  end
  if $arg0 & (1<<48)
    printf "SMI,"
  end
  if $arg0 & (1<<49)
    printf "GPIO0,"
  end
  if $arg0 & (1<<50)
    printf "GPIO1,"
  end
  if $arg0 & (1<<51)
    printf "GPIO2,"
  end
  if $arg0 & (1<<52)
    printf "GPIO3,"
  end
  if $arg0 & (1<<53)
    printf "I2C,"
  end
  if $arg0 & (1<<54)
    printf "SPI,"
  end
  if $arg0 & (1<<55)
    printf "PCM,"
  end
  if $arg0 & (1<<57)
    printf "UART,"
  end
end


define lsirq
  printf "bcm2835 interrupt controller IRQ info:\n"
  set $__basic_pending = *(int*)0x3f00b200
  set $__irq_pending_1 = *(int*)0x3f00b204
  set $__irq_pending_2 = *(int*)0x3f00b208

  set $__basic_enable = *(int*)0x3f00b218
  set $__irq_enable_1 = *(int*)0x3f00b210
  set $__irq_enable_2 = *(int*)0x3f00b214
  printf "ENABLED: bas:%08x,irq1:%08x,irq2:%08x\n", $__basic_enable, $__irq_enable_1, $__irq_enable_2
  __print_irq_basic $__basic_enable
  __print_irq_1 ((uint64_t)$__irq_enable_1) | (((uint64_t)$__irq_enable_2) << 32)
  if $__basic_enable || $__irq_enable_1 || $__irq_enable_2
    printf "\n"
  end

  printf "PENDING: bas:%08x,irq1:%08x,irq2:%08x\n", $__basic_pending, $__irq_pending_1, $__irq_pending_2
  __print_irq_basic $__basic_pending
  __print_irq_1 ((uint64_t)$__irq_pending_1) | (((uint64_t)$__irq_pending_2) << 32)
  if $__basic_pending || $__irq_pending_1 || $__irq_pending_2
    printf "\n"
  end
end

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

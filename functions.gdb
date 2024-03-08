set $spi_cs   = (int *)(0x3f204000)
set $spi_fifo = (int *)(0x3f204004)
set $spi_clk  = (int *)(0x3f204008)
set $spi_dlen = (int *)(0x3f20400c)
set $spi_ltoh = (int *)(0x3f204010)
set $spi_dc   = (int *)(0x3f204014)

define lspi
  printf "SPI_CS:%08x", *$spi_cs
  printf ",CHIPSELECT:%d", *$spi_cs & 3
  if *$spi_cs & (1<<2)
    printf ",CPHA"
  end
  if *$spi_cs & (1<<3)
    printf ",CPOL"
  end
  if *$spi_cs & (1<<6)
    printf ",CSPOL"
  end
  if *$spi_cs & (1<<7)
    printf ",TA"
  end
  if *$spi_cs & (1<<8)
    printf ",DMAEN"
  end
  if *$spi_cs & (1<<9)
    printf ",INTD"
  end
  if *$spi_cs & (1<<10)
    printf ",INTR"
  end
  if *$spi_cs & (1<<11)
    printf ",ADCS"
  end
  if *$spi_cs & (1<<12)
    printf ",REN"
  end
  if *$spi_cs & (1<<13)
    printf ",LEN"
  end
  if *$spi_cs & (1<<16)
    printf ",DONE"
  end
  if *$spi_cs & (1<<17)
    printf ",RXD"
  end
  if *$spi_cs & (1<<18)
    printf ",TXD"
  end
  if *$spi_cs & (1<<19)
    printf ",RXR"
  end
  if *$spi_cs & (1<<20)
    printf ",RXF"
  end
  if *$spi_cs & (1<<21)
    printf ",CSPOL0"
  end
  if *$spi_cs & (1<<22)
    printf ",CSPOL1"
  end
  if *$spi_cs & (1<<23)
    printf ",CSPOL2"
  end
  if *$spi_cs & (1<<24)
    printf ",DMA_LEN"
  end
  if *$spi_cs & (1<<25)
    printf ",LEN_LONG"
  end
  printf "\n"

  if *$spi_cs & (1<<7)
    printf ",transfer active"
  end
  if *$spi_cs & (1<<8)
    printf ",DMA enabled"
  end
  if *$spi_cs & (1<<11)
    printf ",auto de-assert CS"
  end
  if *$spi_cs & (1<<17)
    printf ",RXFIFO not empty"
  end
  if *$spi_cs & (1<<18)
    printf ",TXFIFO not full"
  end
  if *$spi_cs & (1<<19)
    printf ",RXFIFO 3/4 FULL"
  end
  if *$spi_cs & (1<<20)
    printf ",RXFIFO FULL,STALLED"
  end
  printf "\n"

  printf "CLK:%d\n", *$spi_clk
  printf "DLEN:%d\n", *$spi_dlen
end

define spi_fifo_wr_8
  p *(char *)($spi_fifo) = $arg0
  lspi
end

define spi_fifo_wr_32
  p *(int *)$spi_fifo = $arg0
  lspi
end

define dump_dma_cb
  set $__cb_baseaddr = (uint64_t)($arg0) & 0x3fffffff
  set $__cb_ti = *(int*)($__cb_baseaddr + 0x00)
  set $__cb_src = *(int*)($__cb_baseaddr + 0x04)
  set $__cb_dst = *(int*)($__cb_baseaddr + 0x08)
  set $__cb_len = *(int*)($__cb_baseaddr + 0x0c)
  set $__cb_next = *(int*)($__cb_baseaddr + 0x14)
  printf "cb:%08x,TI:%08x,%08x->%08x(%08x),next:%08x,", $arg0, $__cb_ti, $__cb_src, $__cb_dst, $__cb_len, $__cb_next
  if $__cb_ti & (1<<0)
    printf "I"
  else
    printf "-" 
  end
  if $__cb_ti & (1<<3)
    printf "WA"
  else
    printf "--"
  end
  printf "d:"
  if $__cb_ti & (1<<4)
    printf "++"
  else
    printf "  "
  end
  if $__cb_ti & (1<<5)
    printf "128"
  else
    printf " 32"
  end
  if $__cb_ti & (1<<6)
    printf "rq"
  else
    printf "--"
  end
  if $__cb_ti & (1<<7)
    printf "noW"
  else
    printf "---"
  end

  printf "s:"
  if $__cb_ti & (1<<8)
    printf "++"
  else
    printf "  "
  end
  if $__cb_ti & (1<<9)
    printf "128"
  else
    printf " 32"
  end
  if $__cb_ti & (1<<10)
    printf "rq"
  else
    printf "  "
  end
  if $__cb_ti & (1<<11)
    printf "noRD,"
  end
  # Burst
  printf "B%d,", ($__cb_ti >> 12) & 0xf
  printf "P%d,", ($__cb_ti >> 16) & 0x1f
  printf "W%d\n", ($__cb_ti >> 21) & 0x1f

  if $__cb_next
    set $__next_addr = $__cb_next - 0xc0000000
    dump_dma_cb $__next_addr
  end
end

define lsdmach
  printf "dma ch %d:", $arg0
  set $__dma_ch_base = 0x3f007000 + (int)$arg0 * 0x100
  set $__dma_cs    = *(int *)($__dma_ch_base + 0x00)
  set $__dma_cb    = *(int *)($__dma_ch_base + 0x04)
  set $__dma_ti    = *(int *)($__dma_ch_base + 0x08)
  set $__dma_src   = *(int *)($__dma_ch_base + 0x0c)
  set $__dma_dst   = *(int *)($__dma_ch_base + 0x10)
  set $__dma_len   = *(int *)($__dma_ch_base + 0x14)
  set $__dma_next  = *(int *)($__dma_ch_base + 0x1c)
  set $__dma_debug = *(int *)($__dma_ch_base + 0x20)
  printf "CS:%08x,CB:%08x,TI:%08x,%08x->%08x(%08x),next:%08x,DBG:%08x\n", $__dma_cs, $__dma_cb, $__dma_ti, $__dma_src, $__dma_dst, $__dma_len, $__dma_next, $__dma_debug
  if $__dma_cs & (1<<0)
    printf ",ACTIVE"
  end
  if $__dma_cs & (1<<1)
    printf ",END"
  end
  if $__dma_cs & (1<<2)
    printf ",INT(interrupt fired)"
  end
  if $__dma_cs & (1<<3)
    printf ",DREQ active"
  end
  if $__dma_cs & (1<<4)
    printf ",PAUSED"
  end
  if $__dma_cs & (1<<5)
    printf ",PAUSED BY DREQ"
  end
  if $__dma_cs & (1<<6)
    printf ",WAIT FOR OUTWRITE"
  end
  if $__dma_cs & (1<<8)
    printf ",ERROR"
  end
  if $__dma_cs & (1<<28)
    printf ",NEED WAIT FOR OUT WRITE"
  end
  if $__dma_cs & (1<<29)
    printf ",DISDEBUG"
  end
  printf "\n"

  if $__dma_cb
    set $__cbaddr = $__dma_cb - 0xc0000000
    dump_dma_cb $__cbaddr
  end
end

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

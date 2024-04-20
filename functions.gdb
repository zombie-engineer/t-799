set $spi_cs   = (int *)(0x3f204000)
set $spi_fifo = (int *)(0x3f204004)
set $spi_clk  = (int *)(0x3f204008)
set $spi_dlen = (int *)(0x3f20400c)
set $spi_ltoh = (int *)(0x3f204010)
set $spi_dc   = (int *)(0x3f204014)

set $emmc_arg2       = (int *)(0x3f300000)
set $emmc_blksizecnt = (int *)(0x3f300004)
set $emmc_arg1       = (int *)(0x3f300008)
set $emmc_cmdtm      = (int *)(0x3f30000c)
set $emmc_resp0      = (int *)(0x3f300010)
set $emmc_resp1      = (int *)(0x3f300014)
set $emmc_resp2      = (int *)(0x3f300018)
set $emmc_resp3      = (int *)(0x3f30001c)
set $emmc_data       = (int *)(0x3f300020)
set $emmc_status     = (int *)(0x3f300024)
set $emmc_control0   = (int *)(0x3f300028)
set $emmc_control1   = (int *)(0x3f30002c)
set $emmc_interrupt  = (int *)(0x3f300030)
set $emmc_irpt_mask  = (int *)(0x3f300034)
set $emmc_irpt_en    = (int *)(0x3f300038)
set $emmc_control2   = (int *)(0x3f30003c)
set $emmc_force_irpt = (int *)(0x3f300050)
set $emmc_boot_tmout = (int *)(0x3f300070)
set $emmc_dbg_sel    = (int *)(0x3f300074)
set $emmc_exrd_fifo_cfg = (int *)(0x3f300080)
set $emmc_exrd_fifo_en  = (int *)(0x3f300084)
set $emmc_tune_step = (int *)(0x3f300088)
set $emmc_tune_steps_std = (int *)(0x3f30008c)
set $emmc_tune_steps_ddr = (int *)(0x3f300090)
set $emmc_spi_int_spt = (int *)(0x3f3000f0)
set $emmc_slotisr_ver = (int *)(0x3f3000fc)

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

define __print_irq
  set $__irq = (((uint64_t)$arg1) << 32) + $arg0
  if (($__irq >> 0) & 1)
    printf "SYSTIMER_0,"
  end
  if (($__irq >> 1) & 1)
    printf "SYSTIMER_1,"
  end
  if (($__irq >> 2) & 1)
    printf "SYSTIMER_2,"
  end
  if (($__irq >> 3) & 1)
    printf "SYSTIMER_3,"
  end
  if (($__irq >> 16) & 1)
    printf "DMA_CH_0,"
  end
  if (($__irq >> 17) & 1)
    printf "DMA_CH_1,"
  end
  if (($__irq >> 18) & 1)
    printf "DMA_CH_2,"
  end
  if (($__irq >> 19) & 1)
    printf "DMA_CH_3,"
  end
  if (($__irq >> 20) & 1)
    printf "DMA_CH_4,"
  end
  if (($__irq >> 21) & 1)
    printf "DMA_CH_5,"
  end
  if (($__irq >> 22) & 1)
    printf "DMA_CH_6,"
  end
  if (($__irq >> 23) & 1)
    printf "DMA_CH_7,"
  end
  if (($__irq >> 24) & 1)
    printf "DMA_CH_8,"
  end
  if (($__irq >> 25) & 1)
    printf "DMA_CH_9,"
  end
  if (($__irq >> 26) & 1)
    printf "DMA_CH_10,"
  end
  if (($__irq >> 27) & 1)
    printf "DMA_CH_11,"
  end
  if (($__irq >> 28) & 1)
    printf "DMA_CH_12,"
  end
  if (($__irq >> 29) & 1)
    printf "AUX_INT,"
  end
  if (($__irq >> 43) & 1)
    printf "I2C_SPI_SLV_INT,"
  end
  if (($__irq >> 45) & 1)
    printf "PWA0,"
  end
  if (($__irq >> 46) & 1)
    printf "PWA0,"
  end
  if (($__irq >> 48) & 1)
    printf "SMI,"
  end
  if (($__irq >> 49) & 1)
    printf "GPIO0,"
  end
  if (($__irq >> 50) & 1)
    printf "GPIO1,"
  end
  if (($__irq >> 51) & 1)
    printf "GPIO2,"
  end
  if (($__irq >> 52) & 1)
    printf "GPIO3,"
  end
  if (($__irq >> 53) & 1)
    printf "I2C,"
  end
  if (($__irq >> 54) & 1)
    printf "SPI,"
  end
  if (($__irq >> 55) & 1)
    printf "PCM,"
  end
  if (($__irq >> 57) & 1)
    printf "UART,"
  end
  if (($__irq >> 62) & 1)
    printf "ARASAN_SDIO,"
  end
end


define lsirq
  printf "bcm2835 interrupt controller IRQ info:\n"
  set $__basic_pending = *(int*)0x3f00b200
  set $__irq_pending_1 = (uint64_t)(*(int*)0x3f00b204)
  set $__irq_pending_2 = (uint64_t)(*(int*)0x3f00b208)

  set $__basic_enable = *(int*)0x3f00b218
  set $__irq_enable_1 = *(int*)0x3f00b210
  set $__irq_enable_2 = *(int*)0x3f00b214
  printf "ENABLED: bas:%08x,irq1:%08x,irq2:%08x\n", $__basic_enable, $__irq_enable_1, $__irq_enable_2
  __print_irq_basic $__basic_enable
  __print_irq $__irq_enable_1 $__irq_enable_2
  if $__basic_enable || $__irq_enable_1 || $__irq_enable_2
    printf "\n"
  end

  printf "PENDING: bas:%08x,irq1:%08x,irq2:%08x\n", $__basic_pending, $__irq_pending_1, $__irq_pending_2
  __print_irq_basic $__basic_pending
  __print_irq $__irq_pending_1 $__irq_pending_2
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

define lemmc
  printf "EMMC_ARG2: %08x\n", *(int *)$emmc_arg2
  printf "EMMC_BLKSIZECNT: %08x\n", *(int *)$emmc_blksizecnt
  printf "EMMC_ARG1: %08x\n", *(int *)$emmc_arg1
  printf "EMMC_CMDTM: %08x\n", *(int *)$emmc_cmdtm
  printf "EMMC_RESP0: %08x\n", *(int *)$emmc_resp0
  printf "EMMC_RESP1: %08x\n", *(int *)$emmc_resp1
  printf "EMMC_RESP2: %08x\n", *(int *)$emmc_resp2
  printf "EMMC_RESP3: %08x\n", *(int *)$emmc_resp3
  printf "EMMC_DATA: %08x\n", *(int *)$emmc_data
  printf "EMMC_STATUS: %08x\n", *(int *)$emmc_status
  printf "EMMC_CONTROL0: %08x\n", *(int *)$emmc_control0
  printf "EMMC_CONTROL1: %08x\n", *(int *)$emmc_control1
  printf "EMMC_INTERRUPT: %08x\n", *(int *)$emmc_interrupt
  printf "EMMC_IRPT_MASK: %08x\n", *(int *)$emmc_irpt_mask
  printf "EMMC_IRPT_EN: %08x\n", *(int *)$emmc_irpt_en
  printf "EMMC_CONTROL2: %08x\n", *(int *)$emmc_control2
  printf "EMMC_FORCE_IRPT: %08x\n", *(int *)$emmc_force_irpt
  printf "EMMC_BOOT_TMOUT: %08x\n", *(int *)$emmc_boot_tmout
  printf "EMMC_DBG_SEL: %08x\n", *(int *)$emmc_dbg_sel
  printf "EMMC_EXRD_FIFO_CFG: %08x\n", *(int *)$emmc_exrd_fifo_cfg
  printf "EMMC_EXRD_FIFO_EN: %08x\n", *(int *)$emmc_exrd_fifo_en
  printf "EMMC_TUNE_STEP: %08x\n", *(int *)$emmc_tune_step
  printf "EMMC_TUNE_STEPS_STD: %08x\n", *(int *)$emmc_tune_steps_std
  printf "EMMC_TUNE_STEPS_DDR: %08x\n", *(int *)$emmc_tune_steps_ddr
  printf "EMMC_SPI_INT_SPT: %08x\n", *(int *)$emmc_spi_int_spt
  printf "EMMC_SLOTISR_VER: %08x\n", *(int *)$emmc_slotisr_ver
end

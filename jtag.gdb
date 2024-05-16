target remote :3333
set pagination off
set print pretty on
displ/8i $pc
source functions.gdb
p $pc += 4
tb mmu_init
c
fin

source breakpoints.gdb

 #commands
 #silent
 #b bcm2835_emmc_irq_handler
 #commands 3
 #emmc_write 4096 0x40404040
 #end
 #ignore 3 1
 #c
 #end

c

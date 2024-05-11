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
c

target remote :3333
set pagination off
set print pretty on
displ/8i $pc
source functions.gdb
p $pc += 4
tb mmu_init
c
fin
b vchiq_start_thread
b vchiq_parse_msg_openack
# c

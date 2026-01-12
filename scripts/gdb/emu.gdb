target remote :1234
set pagination off
set pri pre on
source functions.gdb
displ/8i $pc
tb debug_wait
c
p $pc += 4

tb vchiq_handmade
tb vchiq_handmade_connect
tb vchiq_loop_thread

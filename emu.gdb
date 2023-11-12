target remote :1234
set pagination off
displ/8i $pc
b debug_wait
c
p $pc += 4
b __exception_handler_sync_curr_sp0
b *0x85880

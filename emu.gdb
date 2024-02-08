target remote :1234
set pagination off
displ/8i $pc
b debug_wait
c
p $pc += 4
# b __clear_bss
b task_create
# b mmu_init
# c

#hb __exception_handler_sync_curr_sp0
#hb __exception_handler_sync_curr_spx
#b aa64_va_parameters
#b get_phys_addr_lpae

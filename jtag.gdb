target remote :3333
set pagination off
set print pretty on
displ/8i $pc
source functions.gdb
p $pc += 4
tb mmu_init
c
fin
b kernel_run
b test_spi
# b ili9341_setup_dma_control_blocks
b ili9341_init

b ili9341_draw_bitmap
# b vchiq_start_thread
 ## b mmal_port_buffer_send_one
 ## b mmal_buffer_print_meta
c

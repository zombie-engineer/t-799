.macro __exception_handler, exception_type, suffix
.align 7
.globl __exception_handler_\()\exception_type\()_\()\suffix
__exception_handler_\()\exception_type\()_\()\suffix:
bl armv8_handle_exception_\()\exception_type
eret
.endm

.macro __exception_vec_group suffix
__exception_handler sync \suffix
__exception_handler irq \suffix
__exception_handler fiq \suffix
__exception_handler serror \suffix
.endm

.section exception_vector, "ax"
.align 11
.globl __exception_vector_table
__exception_vector_table:
/*
 * Exception taken at the same exception level with stack pointer SP0
 */
__exception_vec_group curr_sp0

/*
 * Exception taken at the same exception level with stack pointer SPx
 */
__exception_vec_group curr_spx

/*
 * Exception taken at a lower exception level using Aarch64
 */
__exception_vec_group lower_aarch64

/*
 * Exception taken at a lower exception level using Aarch32
 */
__exception_vec_group lower_aarch32

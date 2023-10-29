.macro __exception_handler, exception_type, suffix
.align 7
.globl __exception_handler_\()\exception_type\()_\()\suffix
__exception_handler_\()\exception_type\()_\()\suffix:
  /*
   * Below code before call to __armv8_cpuctx_store solves the problem that
   * __armv8_store_cpu_context can not be called with BL, because this will
   * change value of x30 (link-register). And the problem is that
   * __armv8_store_cpu_context will not know what should be done after
   * it will finish storing context.
   * Solution is to pass address of next instruction on stack =). Then
   * __armv8_store_cpu_context will first do its job, then it will recover
   * address of next instruction from stack and branch there.
   * In order to do that we need one scratch register, so we are temp storing
   * original value of x0 on stack also and recover it as soon as next
   * instruction is written to stack
   * Initially:
   * SP: XXXXXXXX
   *
   * This is required initially (SP has original value)
   * 2:  SCRATCHR  : sp-16
   * 1:  LINK_REG  : sp-8
   * SP: XXXXXXXX
   *
   * This should be before call to __armv8_store_cpu_context
   * 1:  SCRATCHR  : sp-8
   * SP: LINK_REG  : sp
   * --: XXXXXXXX
   *
   * So we store scratch register at sp-16, link register at sp-8 and dec sp
   * Then scratch register is recovered from stack and we call the function
   * It is assumed the calling function does not corrupt memory under SP
   */
  str   x0, [sp, #-16]
  adr   x0, 1f
  str   x0, [sp, #-8]!
  ldr   x0, [sp, #-8]
  b __armv8_store_cpu_context
1:
  add   sp, sp, #8
  bl armv8_handle_exception_\()\exception_type
  b __armv8_cpuctx_eret
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

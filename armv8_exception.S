.macro GET_CURRENT_CTX dest
  ldr \dest, =__cpu_context
.endm

.macro STORE_REGS_X1_X30, to
  stp   x1 , x2 , [\to], #16
  stp   x3 , x4 , [\to], #16
  stp   x5 , x6 , [\to], #16
  stp   x7 , x8 , [\to], #16
  stp   x9 , x10, [\to], #16
  stp   x11, x12, [\to], #16
  stp   x13, x14, [\to], #16
  stp   x15, x16, [\to], #16
  stp   x17, x18, [\to], #16
  stp   x19, x20, [\to], #16
  stp   x21, x22, [\to], #16
  stp   x23, x24, [\to], #16
  stp   x25, x26, [\to], #16
  stp   x27, x28, [\to], #16
  stp   x29, x30, [\to], #16
.endm

.macro LOAD_REGS_X0_X30, from
  ldp   x0 , x1 , [\from], #16
  ldp   x2 , x3 , [\from], #16
  ldp   x4 , x5 , [\from], #16
  ldp   x6 , x7 , [\from], #16
  ldp   x8 , x9 , [\from], #16
  ldp   x10, x11, [\from], #16
  ldp   x12, x13, [\from], #16
  ldp   x14, x15, [\from], #16
  ldp   x16, x17, [\from], #16
  ldp   x18, x19, [\from], #16
  ldp   x20, x21, [\from], #16
  ldp   x22, x23, [\from], #16
  ldp   x24, x25, [\from], #16
  ldp   x26, x27, [\from], #16
  ldp   x28, x29, [\from], #16
  ldr   x30, [\from]
.endm

.macro ZERO_REGS_X0_X29, __zero_reg, __ctx_reg
  mov   \__zero_reg, #0
  stp   \__zero_reg, \__zero_reg, [\__ctx_reg], #16 // x0, x1
  stp   \__zero_reg, \__zero_reg, [\__ctx_reg], #16 // x2, x3
  stp   \__zero_reg, \__zero_reg, [\__ctx_reg], #16 // x4, x5
  stp   \__zero_reg, \__zero_reg, [\__ctx_reg], #16 // x6, x7
  stp   \__zero_reg, \__zero_reg, [\__ctx_reg], #16 // x8, x9
  stp   \__zero_reg, \__zero_reg, [\__ctx_reg], #16 // x10, x11
  stp   \__zero_reg, \__zero_reg, [\__ctx_reg], #16 // x12, x13
  stp   \__zero_reg, \__zero_reg, [\__ctx_reg], #16 // x14, x15
  stp   \__zero_reg, \__zero_reg, [\__ctx_reg], #16 // x16, x17
  stp   \__zero_reg, \__zero_reg, [\__ctx_reg], #16 // x18, x19
  stp   \__zero_reg, \__zero_reg, [\__ctx_reg], #16 // x20, x21
  stp   \__zero_reg, \__zero_reg, [\__ctx_reg], #16 // x22, x23
  stp   \__zero_reg, \__zero_reg, [\__ctx_reg], #16 // x24, x25
  stp   \__zero_reg, \__zero_reg, [\__ctx_reg], #16 // x26, x27
  stp   \__zero_reg, \__zero_reg, [\__ctx_reg], #16 // x28, x29
.endm

.section .text
.globl __cpu_context
__cpu_context:
.rept 256
.long 0
.endr

.globl __armv8_store_cpu_context
__armv8_store_cpu_context:
  /*
   * Upon entering this functions all cpu context is exactly as it need to be
   * stored.
   * Steps:
   * 1. original x1 is saved to stack
   * 2. x1 is set to destination memory address, to where cpu context will be
   *    written.
   * 3. x0 is written first to cpu_context under x1, x1 is updated to point to
   *    next memory location in cpu_context memory
   * 4. x0 is assigned the value of x1
   * 5. original value of x1 is restored from stack
   * 6. values x1 through x30 are stored to cpu context memory with x0 as a
   *    current memory destination pointer
   * 7  SP_E0, ELR_EL1, SPSR_EL1 are stored in that order
   * 8. x30 is loaded with next instruction address to execute after return
   *    value on stack is set by the caller.
   */
  str x1, [sp, #-8]
  GET_CURRENT_CTX x1
  str x0, [x1], #8
  mov x0, x1
  ldr x1, [sp, #-8]
  STORE_REGS_X1_X30 x0

  /*
   * SP_EL1 holds stack pointer before exception
   * ELR_EL1 holds next PC before exception
   * SPSR_EL1 holds CPSR before exception
   * order of store:
   * [SP_EL0]
   * [ELR_EL1]
   * [SPSR_EL1]
   */
  mrs   x1, sp_el0
  mrs   x2, elr_el1
  mrs   x3, spsr_el1
  stp   x1, x2, [x0], #16
  str   x3, [x0]

  /*
   * Context completely stored, pass execution to instruciton address, written
   * by a caller to sp
   */
  ldr   x30, [sp]
  ret

.globl __armv8_cpuctx_eret
__armv8_cpuctx_eret:
  GET_CURRENT_CTX x30
  /* restore elr and sp0 first */
  ldp   x0 , x1 , [x30, #(31 << 3)]
  msr   sp_el0, x0
  msr   elr_el1, x1
  /* restore spsr */
  ldr   x0 , [x30, #(33 << 3)]
  LOAD_REGS_X0_X30 x30
  eret

.globl armv8_handle_exception_sync
armv8_handle_exception_sync:
  sub sp, sp, #16
  str x30, [sp]
  mrs x0, esr_el1
  bl armv8_exception_handler_sync
  ldr x30, [sp]
  add sp, sp, #16
  ret

.globl armv8_handle_exception_irq
armv8_handle_exception_irq:
bl __handle_irq
ret

.globl armv8_handle_exception_fiq
armv8_handle_exception_fiq:
bl fiq_handler
ret

.globl armv8_handle_exception_serror
armv8_handle_exception_serror:
bl serror_handler
ret

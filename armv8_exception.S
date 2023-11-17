.macro GET_CURRENT_CTX dest
  ldr \dest, =__current_cpuctx
  ldr \dest, [\dest]
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
.globl __cpuctx_stub
__cpuctx_stub:
.rept 256
.long 0
.endr

.globl __current_cpuctx
__current_cpuctx:
.long __cpuctx_stub

/*
 * Called from C, initializes cpu context, arguments:
 * x0 - cpu_context_address
 * x1 - initial program counter
 * x2 - initial stack pointer
 * x3 - initial return address in the link register
 *
 * Order of storage
 * cpuctx + 0  * 8: x0,  x1,  x2,  x3,  x4,  x5,  x6,  x7,  x8,  x9
 * cpuctx + 10 * 8: x10, x11, x12, x13, x14, x15, x16, x17, x18, x19
 * cpuctx + 20 * 8: x20, x21, x22, x23, x24, x25, x26, x27, x28, x29
 * cpuctx + 30 * 8: x30  - link register
 * cpuctx + 31 * 8: sp   - stack pointer
 * cpuctx + 32 * 8: elr  - program counter
 * cpuctx + 33 * 8: spsr - program state
 */
.globl __armv8_cpuctx_init
__armv8_cpuctx_init:
  stp x29, x30, [sp, #-32]!

cpuctx   .req x0
task_pc  .req x1
task_sp  .req x2
task_lr  .req x3
zero_reg .req x4
  ZERO_REGS_X0_X29 zero_reg, cpuctx
.unreq zero_reg
task_pstate .req x4
  mov task_pstate, #0x3c5
  stp task_lr, task_sp, [cpuctx], #16
  stp task_pc, task_pstate, [cpuctx], #16
.unreq task_pstate
.unreq task_lr
.unreq task_sp
.unreq task_pc
.unreq cpuctx
  ldp x29, x30, [sp], #32
  ret

/*
 * __armv8_cpuctx_save - stores CPU context on stack
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
.globl __armv8_cpuctx_save
__armv8_cpuctx_save:
  str x1, [sp, #-8]
tmp_ctx_ptr .req x1
  GET_CURRENT_CTX tmp_ctx_ptr
  str x0, [tmp_ctx_ptr], #8
ctx_ptr .req x0
  mov ctx_ptr, tmp_ctx_ptr
.unreq tmp_ctx_ptr
  ldr x1, [sp, #-8]
ctx_ptr .req x0
  STORE_REGS_X1_X30 ctx_ptr

task_sp .req x1
task_pc .req x2
task_pstate .req x3

  mrs   task_pstate, spsr_el1
  /*
   * PSTATE.SPSel.
   * If 0 - sp is in SP_EL0, easy
   * If 1 - sp is in SP_EL1. If we are in EL1, reading it is not easy
   *        but also if we are in EL1, sp is current sp, before modifiactions
   *        if we are here SP value is modified by the called by 16 bytes
   *        so we just un-modify it back
   */
  tst task_pstate, #1
  beq 1f
  add task_sp, sp, #16
  b 2f
1:
  mrs   task_sp, sp_el0
2:
  mrs   task_pc, elr_el1
  stp   task_sp, task_pc, [ctx_ptr], #16
.unreq task_sp
.unreq task_pc
  str   task_pstate, [ctx_ptr]
.unreq task_pstate
  /*
   * Context completely stored, pass execution to instruciton address, written
   * by a caller to sp
   */
orig_lr .req x30
  ldr   orig_lr, [sp]
.unreq orig_lr
  ret

.globl __armv8_cpuctx_eret
/*
 * x0,  x1,  x2,  x3,  x4,  x5,  x6,  x7,  x8,  x9
 * x10, x11, x12, x13, x14, x15, x16, x17, x18, x19
 * x20, x21, x22, x23, x24, x25, x26, x27, x28, x29
 * x30  - link register
 * sp   - stack pointer
 * elr  - program counter
 * spsr - program state
 */
__armv8_cpuctx_eret:
ctx_ptr     .req x30
task_pstate .req x0
task_sp     .req x1
task_pc     .req x2

  GET_CURRENT_CTX x30
  /* restore spsr */
  ldp task_sp, task_pc, [x30, #(31 * 8)]
  ldr task_pstate,      [x30, #(33 * 8)]
  tst task_pstate, #1
  beq 1f
  mov sp, task_sp
  b 2f
1:
  msr sp_el0, task_sp

2:
.unreq task_sp
  msr spsr_el1, task_pstate
.unreq task_pstate
  msr elr_el1 , task_pc
.unreq task_pc
  LOAD_REGS_X0_X30 x30
.unreq ctx_ptr
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

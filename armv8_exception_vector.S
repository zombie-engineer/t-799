#define BCM2835_IC_BASE 0x3f00b200

#define ARM_IRQ1_BASE 0
#define ARM_IRQ2_BASE 32
#define ARM_BASIC_BASE 64

.equ IC_BASE, BCM2835_IC_BASE
.equ BASIC_PENDING_REG, 0x0
.equ PENDING_REG_1, 0x4
.equ PENDING_REG_2, 0x8

/*
 * Macros get_irqrn_preamble and get_irqnr_and_base are
 * copied from BCM2835 ARM Peripherals datasheet, page 111
 *
 * Concept:
 * Register BASIC_PENDING_REG (0x7e00b200 or 0x3f00b200) is an optimization to
 * know summary of all pending interrupts by only reading one register.
 * Interrupts are arranged into 3 groups - ARM, GPU and VC irqs, each presented
 * by own register. BASIC PENDING holds info about ARM  group but also has
 * summary of 2 others. Optimization is in utilizing free bits of
 * BASIC_PENDING_REG register, while only 8 bits are assigned to ARM irqs, the
 * rest is utilized to show other group irqs.
 * Bit 8 shows IRQ_GPU0_PENDING is non zero.
 * Bit 9 shows same for PENDING_REG_1.
 * Bits 10 to 20 are mapped to the most frequent IRQs from GPU0/GPU1 pending
 * registers.If they are set, bits 8 and 9 will not be set to represent them.
 *
 * 'get_irqrn_preamble' is initialization macro-API that inits base register
 * address.
 * 'get_irqnr_and_base' actually sets the irqnr register with first met pending
 * IRQ bit and sets Z (zero flag) in CPU status register when no pending irqs
 * are present. So this macro should be used in a while loop with beq leading to
 * loop-break when all interrupts are processed and cleared.
 */

.macro get_irqrn_preamble, base
  mov   \base, (BCM2835_IC_BASE & 0xffff0000)
  movk  \base, (BCM2835_IC_BASE & 0xffff)
.endm

.macro get_irqnr_and_base irqnr, irqstat, base, tmp, tmp2, addr_reg64
  /* get masked status */
  ldr   \irqstat, [\base, #BASIC_PENDING_REG]
  cmp   \irqstat, #0
  beq get_irqnr_and_base_end_\@

  /*
   * We want to know the number of the rightmost set bit in a 32 bit word
   * We apply clz to value of basic pending interrupt, CLZ - count leading
   * zeroes. We then mast invert this by subtracting result from 31.
   * 31 - CLZ(irqstat)
   */
  clz   \irqstat, \irqstat
  mov   \tmp, #31
  sub   \irqnr, \tmp, \irqstat

  /*
   * Check if the rightmost set bit in the basic pending register is  above 9,
   * then we want to convert bit index to actual interrupt number by a special
   * mapping table at basic_reg_fast_irq_lookup_
   */
  cmp   \irqnr, #9
  bgt   basic_reg_fast_irq_lookup_\@

  /*
   * We are here if the righmost set bit in the basic pending register is not
   * above 9. We then check if it is actually 9. This bit means we have to
   * check pending register 2, which is one of the GPU interrupts
   */
  beq get_irqnr_and_base_test_bit_9_\@

  /*
   * We are here if the righmost set bit in the basic pending register is
   * below 9. We then check if it is actually 8. This bit means we have to
   * check pending register 1, which is one of the GPU interrupts
   */
  cmp \irqnr, #8
  beq get_irqnr_and_base_test_bit_8_\@

  /*
   * We are here if the righmost set bit in the basic pending register is
   * below 8. This means it is one of the 8 ARM interrupts, their numbers
   * correspond to actual bit indexes of the basic pending register. In that
   * case irqnr points to the rightmost set bit index aka ARM interrupt number
   * while zero-flag is not set, indicating there is an interrupt
   */
  add \irqnr, \irqnr, #64
  b get_irqnr_and_base_end_\@

basic_reg_fast_irq_lookup_\@:
  ldr   \addr_reg64, =get_irqnr_basic_map_\@
  sub   \irqnr, \irqnr, #10
  add   \addr_reg64, \addr_reg64, \irqnr, uxtw
  ldrb  \irqnr, [\addr_reg64]
  cmp   \irqnr, #0
  b get_irqnr_and_base_end_\@

get_irqnr_and_base_test_bit_8_\@:
  ldr   \irqstat, [\base, #PENDING_REG_1]

  /* Mask bits, present in BASIC_PENDING_REG */
  mov   \tmp2, #((1<<7)|(1<<9)|(1<<10))
  orr   \tmp2, \tmp2, #((1<<18)|(1<<19))
  bic   \irqstat, \irqstat, \tmp2

  /* Get rightmost set bit index in PENDING_REG_1 */
  clz   \irqstat, \irqstat
  mov   \tmp, #31
  subs  \irqnr, \tmp, \irqstat
  b get_irqnr_and_base_end_\@

get_irqnr_and_base_test_bit_9_\@:
  ldr   \irqstat, [\base, #PENDING_REG_2]

  /* Mask bits, present in BASIC_PENDING_REG */
  mov   \tmp2, #((1<<21)|(1<<22)|(1<<23)|(1<<24)|(1<<25))
  orr   \tmp2, \tmp2, #(1<<30)
  bic   \irqstat, \irqstat, \tmp2

  /* Get rightmost set bit index in PENDING_REG_2 */
  clz   \irqstat, \irqstat
  mov   \tmp, #31
  subs  \irqnr, \tmp, \irqstat
  /* If no bits are set zero flag is set, irqnr is 0, so we exit */
  beq get_irqnr_and_base_end_\@

  /* If any bits are set, convert bit index to IRQ number for PENDING_IRQ2 */
  add   \irqnr, \irqnr, #32
  b get_irqnr_and_base_end_\@

get_irqnr_basic_map_\@:
# Mapping of "basic pending register" described in "BCM2835 ARM Peripherals"
# interrupts section
.byte 7
.byte 9
.byte 10
.byte 18
.byte 19
.byte 53
.byte 54
.byte 55
.byte 56
.byte 57
.byte 62
.byte 0xff

get_irqnr_and_base_end_\@:
.endm

.macro __exception_handler, exception_type, suffix
.align 7
.globl __exception_handler_\()\exception_type\()_\()\suffix
__exception_handler_\()\exception_type\()_\()\suffix:
  /*
   * Below code before call to __armv8_cpuctx_save solves the problem
   * that __armv8_cpuctx_save can not be called with BL, because this
   * will change value of x30 (link-register). And the problem is that
   * __armv8_cpuctx_save will not know what should be done after
   * it will finish storing context.
   * Solution is to pass address of next instruction on stack =). Then
   * __armv8_cpuctx_save will first do its job, then it will recover
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
   * This should be before call to __armv8_cpuctx_save
   * 1:  SCRATCHR  : sp-8
   * SP: LINK_REG  : sp
   * --: XXXXXXXX
   *
   * So we store scratch register at sp-16, link register at sp-8 and dec sp
   * Then scratch register is recovered from stack and we call the function
   * It is assumed the calling function does not corrupt memory under SP
   */
  sub   sp, sp, #16
  str   x0, [sp, #8]
  adr   x0, 1f
  str   x0, [sp]
  ldr   x0, [sp, #8]
  b __armv8_cpuctx_save
1:
  add   sp, sp, #16
  bl armv8_handle_exception_\()\exception_type
  b __armv8_cpuctx_restore
.endm

.macro __exception_handler_irq suffix
.align 7
.globl __exception_handler_irq_\()\suffix
__exception_handler_irq_\()\suffix:
  sub   sp, sp, #16
  str   x0, [sp, #8]
  adr   x0, 1f
  str   x0, [sp]
  ldr   x0, [sp, #8]
  b __armv8_cpuctx_save
1:
  add   sp, sp, #16

/*
 * x19 to x29 are callee-saved registers, so
 * we can safely use them across subroutine calls.
 */

/*
 * base_reg is the only one that needs 64 bit register
 * for load operations.
 */
base_reg .req x19

/*
 * temporary registers don't need to be saved after
 * subroutine call, obviously
 */

/*
 * irqstat is a temp register with name)
 */
irqstat_reg .req w3

/*
 * irqnr will be passed to handler, so it is
 * also corruptable. It's used as first argument,
 * so it has to be w0
 */

irqnr_reg .req w0

/*
 * temp registers
 */
tmp_1_reg .req w4
tmp_2_reg .req w5
tmp_3_reg .req x6

  get_irqrn_preamble base_reg
1:
  get_irqnr_and_base irqnr_reg, irqstat_reg, base_reg, tmp_1_reg, tmp_2_reg, tmp_3_reg
  beq  1f
  bl   __handle_irq
  b    1b
1:
  b     __armv8_cpuctx_restore
.unreq base_reg
.endm

.macro __exception_vec_group suffix
__exception_handler sync \suffix
.align 7
b __exception_handler_irq_\()\suffix
__exception_handler fiq \suffix
__exception_handler serror \suffix
.endm

.section .exception_vector, "ax"
.align 11
.globl __exception_vector_table_el1
__exception_vector_table_el1:
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

.section .exception_vector, "ax"
.align 11
.globl __exception_vector_table_el2
__exception_vector_table_el2:
__exception_vec_group curr_sp0_el2
__exception_vec_group curr_spx_el2
__exception_vec_group lower_aarch64_el2
__exception_vec_group lower_aarch32_el2

__exception_handler_irq curr_sp0
__exception_handler_irq curr_spx
__exception_handler_irq lower_aarch64
__exception_handler_irq lower_aarch32
__exception_handler_irq curr_sp0_el2
__exception_handler_irq curr_spx_el2
__exception_handler_irq lower_aarch64_el2
__exception_handler_irq lower_aarch32_el2

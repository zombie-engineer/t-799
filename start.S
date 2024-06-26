.section .stacks, "a"
.align 4
.globl __stack_el0
__stack_el0:
.rept 512
.long 0x13131313
.endr
.align 16
.globl __stack_el1
__stack_el1:
.rept 512
.long 0x00112233
.endr
.align 4
.globl __stack_el2
__stack_el2:
.rept 512
.long 0x00112233
.endr

#.section .stacks_no_mmu, "a"
#.globl stack_no_mmu
#stack_no_mmu:
#.rept 256
#.quad 0x1122334455667788
#.endr


.section .init.nommu
.globl _start
_start:
  mrs x0, mpidr_el1
  and x0, x0, #0xff
  cbz x0, start_primary_cpu
1:
  wfe
  b 1b

start_primary_cpu:
  /*
   * Set exception vector table
   */
  ldr   x0, =__exception_vector_table_el1
  msr   vbar_el1, x0
  ldr   x0, =__exception_vector_table_el2
  msr   vbar_el2, x0

  /*
   * EL0VCTEN == 1 - don't trap EL0 access to CNTVCT_EL0, CNTFRQ_EL0 to EL2
   * EL0PCTEN == 1 - don't trap EL0 access to CNTPCT_EL0, CNTFRQ_EL0 TO EL2
   */
  mrs   x0, cnthctl_el2
  orr   x0, x0, #3
  msr   cnthctl_el2, x0

  /*
   * Set counter virtual offset to 0
   */
  msr   cntvoff_el2, xzr

  /*
   * Disable floating point registers trap CPACR_EL1.FPEN
   */
  mrs   x0, cpacr_el1
  orr   x0, x0, #(3<<20)
  msr   cpacr_el1, x0
  /*
   * HCR_EL2.RW - 1 - EL1 is running in Aarch64 execution state
   * HCR_EL2.SWIO - 1 - DC ISW does same as DC CISW
   */
  mrs   x0, hcr_el2
  orr   x0, x0, #(1 << 31)
  orr   x0, x0, #(1 << 1)
  msr   hcr_el2, x0

  /*
   * Setup SCTLR aaccess
   * 0 - MMU disabled 
   * 1 - Alignment enabled
   * 2 - D - data cache disabled
   * 3 - SA - EL1 stack alignment check enabled
   * 4 - SP - EL0 stack alignment check enabled
   * 5 - CP15BEN Enable mem barrier instructions at EL0 AArch32
   * 11 - EOS - exeption exit is context synchronizing
   * 12 - I - instruction cache disabled
   * 22 - EIS - exception entry is context synchronizing
   * 23 - SPAN - PSTATE.PAN unchanged when taken to EL1
   * 28 - nTLSMD - allow EL0 to access device memory
   * 29 - LSMAOE - load store 
   */
  mov   x0, #((1<<11) | (1<<1) | (1<<3) | (1<<4) | (1<<5))
  movk  x0, #((1<<(20-16))|(1<<(22-16))|\
              (1<<(23-16))|(1<<(28-16))|\
              (1<<(29-16))), lsl #16
  msr   sctlr_el1, x0

  /*
   * Set stack pointer for EL1
   */
  ldr   x0,=__stack_el1
  msr   sp_el1, x0

  /*
   * Change execution level to EL1.
   * Set SPSR_EL2 to simulate exception context to which to return
   * spsr_el2 = 0x3c5
   * SPSR_EL2.M = EL1 (0b0100) - exception came from EL1
   * SPSR_EL2.F = 1 - FIQ interrupts will be masked upon returning to EL1
   * SPSR_EL2.I = 1 - IRQ interrupts will be masked upon returning to EL1
   * SPSR_EL2.A = 1 - SError interrupts will be masked upon returning to EL1
   * SPSR_EL2.D = 1 - Debug interrupts will be masked upon returning to EL1
   */
  mov   x0, #0x3c5
  msr   spsr_el2, x0

debug_wait:
  nop
#  1: b 1b

  /*
   * Set ELR_EL2 to point to __start_el1_nommu from where it proceeds execution
   */
#  adr   x0, __stack_el1
  adr   x0, __stack_no_mmu_base
  msr   sp_el1, x0
  adr   x0, __start_el1_nommu
  msr   elr_el2, x0
  eret
1:
  mov x0, x0
  wfe
  b 1b

__start_el1_nommu:
  mov   x0, #0x3c5
  msr   spsr_el1, x0
  ldr x0, =__dma_memory_start_phys
  ldr x1, =__dma_memory_end_phys
  bl mmu_init
  ldr x0, =__start_el1_mmu
  blr x0
.section .start_el1_mmu
__start_el1_mmu:
  mrs   x0, currentel
  lsr   x0, x0, #2
  ldr   x1, =__bss_start
  ldr   x2, =__bss_end
  mov   x3, #0
  mov   x4, #0
  cmp   x1, x2
  beq   __run_main
__clear_bss:
  stp x3, x4, [x1], #16
  cmp   x1, x2
  bne __clear_bss
__run_main:
  bl main
##
#  adr   x0, __stack_el0
#  msr   sp_el0, x0
#  adr   x0, __start_el0
#  msr   elr_el1, x0
#  eret
#
#__start_el0:
#  mrs   x1, currentel
#  lsr   x1, x1, #2
#  ldr   x0, =__bss_start
#  ldr   x2, =__bss_end
#  bl main

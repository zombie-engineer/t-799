.section .text
/*
 * dcache_clean_and_invalidate_rng - cleans and invalidates
 * all data cache lines that form whole virtual address range
 * given by the arguments.
 * x0 - virtual address of first byte in range
 * x1 - virtual address of end of range
 */
.globl dcache_clean_and_invalidate_rng
dcache_clean_and_invalidate_rng:
  /*
   * 1. Get line width from
   * CTR_EL0 DminLine [16:19]
   * Log2 of number of words in the smallest cache line
   */
  mrs  x3, CTR_EL0
  lsr  x3, x3, #16
  and  x3, x3, #0xf
  mov  x2, #4
  lsl  x2, x2, x3
  /* Now we know cache line size in x2 */
  sub x3, x2, #1
                       // x0 = aligned(start)
                       // align address to cache line offset
  bic x0, x0, x3   // x0 = start & ~(4^(DminLine) - 1)
1:
  dc  civac, x0    // clean & invalidate data or unified cache
  add x0, x0, x2   // x0 += DminLine
  cmp x0, x1       // check end reached
  b.lo  1b
  dsb sy
  ret

.globl dcache_invalidate_rng
dcache_invalidate_rng:
  /*
   * 1. Get line width from
   * CTR_EL0 DminLine [16:19]
   * Log2 of number of words in the smallest cache line
   */
  mrs  x3, CTR_EL0
  lsr  x3, x3, #16
  and  x3, x3, #0xf
  mov  x2, #4
  lsl  x2, x2, x3
  /* Now we know cache line size in x2 */
  sub x3, x2, #1
                       // x0 = aligned(start)
                       // align address to cache line offset
  bic x0, x0, x3   // x0 = start & ~(4^(DminLine) - 1)
1:
  dc  ivac, x0    // clean & invalidate data or unified cache
  add x0, x0, x2   // x0 += DminLine
  cmp x0, x1       // check end reached
  b.lo  1b
  dsb sy
  ret

.globl get_cpu_counter_64_freq
get_cpu_counter_64_freq:
  mrs x0, cntfrq_el0
  ubfx x0, x0, #0, #32
  ret

.globl read_cpu_counter_64
read_cpu_counter_64:
  mrs x0, cntpct_el0
  ret

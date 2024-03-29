#define LOCKED 1

.globl atomic_cmp_and_swap
atomic_cmp_and_swap:
  str x3, [sp, #-16]!
  mov x3, x0
1:
  ldxr x0, [x3]
  cmp x0, x1
  bne 2f
  stxr w0, x2, [x3]
  cbnz w0, 1b
  mov x0, x1
2:
  ldr x3, [sp], #16
  ret

.globl atomic_inc
atomic_inc:
1:
  ldxr  x1, [x0]
  add   x1, x1, #1
  stxr  w2, x1, [x0]
  cbnz  w2, 1b
  mov   x0, x1
  ret

.globl atomic_dec
atomic_dec:
1:
  ldxr  x1, [x0]
  sub   x1, x1, #1
  stxr  w2, x1, [x0]
  cbnz  w2, 1b
  mov   x0, x1
  ret

.globl __armv8_spinlock_init
__armv8_spinlock_init:
  str xzr, [x0]
  ret

.globl __armv8_spinlock_lock
__armv8_spinlock_lock:
  mov   x1, #LOCKED
  /* Mark address to exclusive monitor */
  ldxr  x2, [x0]
  /* If not zero (LOCKED by other), goto wait */
  cbnz  x2, 1f
  /* If zero (not LOCKED), try to set LOCKED state to that address */
  stxr  w3, x1, [x0]

  /*
   * stxr sets store result to w3
   * if w3 == 0, store completed with success.
   * if w3 == 1, store failed. This can happen
   * if for example we context switched right
   * before the store operation. This effectively
   * cleared exclusive state and some other process
   * used ldxr to reown the access.
   * in this case we go to sleep until next time
   */
  cbnz  w3, 1f

  /*
   * if w3 is 0 - we are ready to enter critical section
   * barrier is to order all memory accesses in critical
   * section after LOCK was stored
   */
  dmb   sy
  ret
1:
  /* sleep */
  wfe
  b __armv8_spinlock_lock

.globl __armv8_spinlock_unlock
__armv8_spinlock_unlock:
  str   xzr, [x0]
  /*
   * After we stored unlocked state to
   * the address we put memory synchronization
   * barrier to make sure SEV is executed
   * AFTER store operation.
   */
  dsb   st
  sev
  ret

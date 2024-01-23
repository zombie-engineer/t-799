.globl atomic_cmp_and_swap
atomic_cmp_and_swap:
  str x3, [sp, #-8]!
  mov x3, x0
1:
  ldxr x0, [x3]
  cmp x0, x1
  bne 2f
  stxr w0, x2, [x3]
  cbnz w0, 1b
  mov x0, x1
2:
  ldr x3, [sp], #8
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

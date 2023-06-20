.section .bootload
.global _bootload_start
_bootload_start:

.section .text.startup
.globl _start
_start:
  mrs x0, mpidr_el1
  and x0, x0, #0xff
  cbz x0, start_primary_cpu
1:
  wfe
  b 1b

start_primary_cpu:
  mov x0, #0xf7
  mov sp, #0x80000
  bl main
1:
  mov x0, x0
  wfe
  b 1b

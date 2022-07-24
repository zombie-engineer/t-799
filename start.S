.section .bootload
.global _bootload_start
_bootload_start:

.section .text
.globl _start
_start:

mov x0, #0xf7
bl kernel_c_entry
mov x0, x0

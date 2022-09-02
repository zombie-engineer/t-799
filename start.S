.section .bootload
.global _bootload_start
_bootload_start:

.section .text
.globl _start
_start:

mov x0, #0xf7
mov sp, #0x80000
bl kernel_c_entry
1:
mov x0, x0
b 1b

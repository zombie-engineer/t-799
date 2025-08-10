.section .exception_vector
.globl armv8_handle_exception_sync
armv8_handle_exception_sync:
  sub sp, sp, #16
  str x30, [sp]
  mrs x0, esr_el1
  bl armv8_exception_handler_sync
  ldr x30, [sp]
  add sp, sp, #16
  ret

.globl armv8_handle_exception_fiq
armv8_handle_exception_fiq:
bl fiq_handler
ret

.globl armv8_handle_exception_serror
armv8_handle_exception_serror:
bl serror_handler
ret

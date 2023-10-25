.globl armv8_handle_exception_sync
armv8_handle_exception_sync:
mrs   x0, esr_el1
b armv8_exception_handler_sync
ret

.globl armv8_handle_exception_irq
armv8_handle_exception_irq:
bl irq_handler
ret

.globl armv8_handle_exception_fiq
armv8_handle_exception_fiq:
bl fiq_handler
ret

.globl armv8_handle_exception_serror
armv8_handle_exception_serror:
bl serror_handler
ret

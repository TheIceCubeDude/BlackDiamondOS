global unhandled_int_wrapper
global exception_wrapper
global exception_wrapper_end

global enable_irqs
global disable_irqs
global set_idt

extern unhandled_int
extern exception_handler

exception_wrapper:
save_regs
mov rdi, 0 ;;Patched at runtime
mov rsi, 0x1234567890ABCDEF ;;Patched at runtime
call rsi
cmp rax, 1
je .pop_err_code
restore_regs
iretq
.pop_err_code:
restore_regs
sub rsp, 8
iretq
exception_wrapper_end:

unhandled_int_wrapper:
save_regs
call near unhandled_int
restore_regs
iretq

enable_irqs:
sti
ret

disable_irqs:
cli
ret

set_idt:
lidt[rdi]
ret

[BITS 64]

global _start
global logo
global font
global halt
extern kmain

_start:
;;This will be overwritten by a memory block header
jmp near start
times 32-($-_start) db 0

start:
mov rdi, rax
mov rsi, rbx
push rdx 
mov rdx, rcx
pop rcx
call kmain

halt:
cli
hlt
jmp halt

%include "kernel/core/ports.asm"
%include "kernel/core/mem.asm"

SECTION .data
logo: incbin "kernel/res/logo.tga"
font: incbin "kernel/res/font.psf"

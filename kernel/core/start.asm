[BITS 64]

global _start
global logo
global font
global halt
extern kmain

_start:
mov rdi, rax
mov rsi, rbx
push rdx 
mov rdx, rcx
pop rcx
call kmain

;;Utils
halt:
cli
hlt
jmp halt

%macro save_regs 0
push rsp
push rax
push rbx
push rcx
push rdx
push rdi
push rsi
push rbp
push r8
push r9
push r10
push r11
push r12
push r13
push r14
push r15
%endmacro
%macro restore_regs 0
pop r15
pop r14
pop r13
pop r12
pop r11
pop r10
pop r9
pop r8
pop rbp
pop rsi
pop rdi
pop rdx
pop rcx
pop rbx
pop rax
pop rsp
%endmacro

%include "kernel/core/ports.asm"
%include "kernel/core/mem.asm"
%include "kernel/core/interrupts.asm"

SECTION .data
logo: incbin "kernel/res/logo.tga"
font: incbin "kernel/res/font.psf"

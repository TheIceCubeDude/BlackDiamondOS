global memset
global memcpy

memset:
;;RDI = dest ptr
;;RSI = src u8
;;RDX = count u64
push rbx
push r12
push r13
push r14
push r15
mov r8, rdx
mov rcx, rsi
.count_check:
;;Check if count is divisible by 16, otherwise memset the remainder
mov rax, r8
xor rdx, rdx
mov rbx, 16
div rbx
sub r8, rdx
.fill:
test rdx, rdx
jz .prep_loop
mov [rdi], cl
inc rdi
dec rdx
jmp .fill
.prep_loop:
;;Prepare the xmm0 register by filling it with the src byte 16 times
mov ch, cl
mov ax, cx
shl ecx, 16
mov cx, ax
movd xmm0, ecx
unpcklps xmm0, xmm0
unpcklpd xmm0, xmm0
.align_check:
;;Check if dest is aligned on a 16 byte boundry so we can use aligned SSE instructions
mov rax, rdi
mov rbx, 16
xor rdx, rdx
div rbx
mov rcx, rsi
test rdx, rdx
jz .loop_aligned
.loop_unaligned:
;;Loop through and fill in the data from the xmm0 register
test r8, r8
jz .end
movdqu [rdi], xmm0
add rdi, 16
sub r8, 16
jmp .loop_unaligned
.loop_aligned:
;;Loop through and fill in the data from the xmm0 register
test r8, r8
jz .end
movdqa [rdi], xmm0
add rdi, 16
sub r8, 16
jmp .loop_aligned
.end:
pop r15
pop r14
pop r13
pop r12
pop rbx
ret

memcpy: 
;;RDI = dest ptr
;;RSI = src ptr
;;RDX = count u64
push rbx
push r12
push r13
push r14
push r15
mov r8, rdx
.count_check:
;;Check if count is divisible by 16, otherwise memset the remainder
mov rax, r8
xor rdx, rdx
mov rbx, 16
div rbx
sub r8, rdx
.fill:
test rdx, rdx
jz .dest_align_check
mov cl, [rsi]
mov [rdi], cl
inc rdi
inc rsi
dec rdx
jmp .fill
.dest_align_check:
;;Check if the destination is divisble by 16 so we can use aligned SSE instructions
mov rax, rdi
xor rdx, rdx
mov rbx, 16
div rbx
test rdx, rdx
jnz .no_dest_align
.dest_align: mov r9, 1
jmp .src_align_check
.no_dest_align: mov r9, 0
.src_align_check:
;;Check if the source is divisble by 16 so we can use aligned SSE instructions
mov rax, rsi
xor rdx, rdx
mov rbx, 16
div rbx
test rdx, rdx
jnz .no_src_align
.src_align: mov r10, 1
jmp .select_loop_1
.no_src_align: mov r10, 0
;;Determine which loop to use
.select_loop_1:
mov rcx, 16
test r9, r9
jz .select_loop_3
.select_loop_2:
test r10, r10
jz .loop_dest_aligned_src_unaligned
jmp .loop_dest_aligned_src_aligned
.select_loop_3:
test r10, r10
jz .loop_dest_unaligned_src_unaligned
jmp .loop_dest_unaligned_src_aligned
.loop_dest_aligned_src_unaligned:
test r8, r8
jz .end
movdqu xmm0, [rsi]
movdqa [rdi], xmm0
add rsi, rcx
add rdi, rcx
sub r8, rcx
jmp .loop_dest_aligned_src_unaligned
.loop_dest_aligned_src_aligned:
test r8, r8
jz .end
movdqa xmm0, [rsi]
movdqa [rdi], xmm0
add rsi, rcx
add rdi, rcx
sub r8, rcx
jmp .loop_dest_aligned_src_aligned
.loop_dest_unaligned_src_unaligned:
test r8, r8
jz .end
movdqu xmm0, [rsi]
movdqu [rdi], xmm0
add rsi, rcx
add rdi, rcx
sub r8, rcx
jmp .loop_dest_unaligned_src_unaligned
.loop_dest_unaligned_src_aligned:
test r8, r8
jz .end
movdqa xmm0, [rsi]
movdqu [rdi], xmm0
add rsi, rcx
add rdi, rcx
sub r8, rcx
jmp .loop_dest_unaligned_src_aligned
.end:
pop r15
pop r14
pop r13
pop r12
pop rbx
ret

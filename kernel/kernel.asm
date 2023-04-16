[BITS 32]
mov eax, 0xDEADBEEF
jmp near test
times 1024*1024 db 0
test:
cli
hlt

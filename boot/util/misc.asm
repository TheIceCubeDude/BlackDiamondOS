print_string:
;;ECX = str
mov ah, 0x0E
mov al, [ecx]
cmp al, 0
je .ret
xor bx, bx
int 0x10
inc ecx
jmp print_string
.ret: ret

align 4
DAP:
.size: db 0x10
.unused: db 0
.count: dw 1
.offset: dw 0x7E00
.segment: dw 0
.lba: dq 0

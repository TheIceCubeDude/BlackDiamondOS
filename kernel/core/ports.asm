global inb
global inw
global inl
global outb
global outw
global outl

inb:
mov dx, di
xor eax, eax
in al, dx
mov edx, 1
ret

inw:
mov dx, di
xor eax, eax
in ax, dx
mov edx, 1
ret

inl:
mov dx, di
xor eax, eax
in eax, dx
mov edx, 1
ret

outb:
mov dx, di
mov ax, si
out dx, al
ret

outw:
mov dx, di
mov ax, si
out dx, ax
ret

outl:
mov dx, di
mov ax, si
out dx, eax
ret

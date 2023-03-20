[BITS 16]
[ORG 0x8000]

enable_A20:
mov eax, 0xDEADBEEF
jmp halt
mov ecx, ENABLE_A20_STR
;call print_string

ENABLE_A20_STR: db 0xA, 0xD, "-> Enabling A20 line...", 0
times (4 * 512) - ($-$$) db 0
halt: hlt
times (5 * 512) - ($-$$) db 0

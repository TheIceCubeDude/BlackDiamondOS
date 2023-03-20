[BITS 16]
[ORG 0x8000]

start:
call ok
xor cx, cx
xor edx, edx

enable_A20:
mov ecx, ENABLE_A20_STR
call print_string
.check:
;;Check if the A20 line is not enabled by checking for wraparound of the bootsig (0x55AA) at the 1M mark
inc edx
mov ax, 0xFFFF
mov es, ax
mov ax, [es:0x7E0E]
xor bx, bx
mov es, bx
cmp [0x7DFE], ax
jne .end
cmp edx, 2097152 ;;Enabling may take some time (this is the timeout value Linux uses)
jne .check
xor edx, edx
cmp cx, 1 ;;We have tried bios_enable
je .ps2_enable
cmp cx, 2 ;;We have tried ps2_enable
je .fast_enable
cmp cx, 3 ;;We have tried fast_enable
je fail
.bios_enable:
mov ax, 0x2401
int 0x15
mov cx, 1
jmp .check
.ps2_enable:
call .ps2_enable_wait_in ;;Disable first port
mov al, 0xAD
out 0x64, al
call .ps2_enable_wait_in ;;Read controller output port
mov al, 0xD0
out 0x64, al
call .ps2_enable_wait_out
in al, 0x60
push ax
call .ps2_enable_wait_in ;;Write controller output port with A20 bit enabled
mov al, 0xD1
out 0x64, al
call .ps2_enable_wait_in
pop ax
or al, 2
out 0x60, al
call .ps2_enable_wait_in ;;Enable first port
mov al, 0xAE
out 0x64, al
call .ps2_enable_wait_in
mov cx, 2
jmp .check
.fast_enable:
mov cx, 3
in al, 0x92
test al, 2
jnz .check ;;Fast A20 already enabled???
or al, 2
and al, 0xFE ;;Make sure bit 0 is not one (causes a system reset)
out 0x92, al
jmp .check
.ps2_enable_wait_in:
in al, 0x64
test al, 2
jnz .ps2_enable_wait_in
ret
.ps2_enable_wait_out:
in al, 0x64
test al, 1
jz .ps2_enable_wait_out
ret
.end:
call ok

hlt
get_memory_map:
load_kernel:
get_vbe_modes:
set_vbe_mode:
enter_pmode:

;;Useful subroutines
fail:
mov ecx, FAIL_STR
call print_string
hlt

ok:
mov ecx, OK_STR
call print_string
ret

print_string:
;;ECX = str
mov ah, 0xE
mov al, [ecx]
cmp al, 0
je ret
xor bx, bx
int 0x10
inc ecx
jmp print_string

ret: ret

OK_STR: db "OK", 0
FAIL_STR: db "FAIL", 0
ENABLE_A20_STR: db 0xA, 0xD, "-> Enabling A20 line...", 0
times (5 * 512) - ($-$$) db 0

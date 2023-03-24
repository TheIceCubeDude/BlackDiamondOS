[BITS 16]
[ORG 0x8000]

;;The second stage:
;;1. Retrieves needed information for the kernel
;;2. Loads kernel
;;3. Enters the best avaliable video mode
;;4. Enters protected mode
;;5. Jumps to kernel

start:
cli
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
and al, 0xFE ;;Make sure bit 0 is not one (causes a system reset), because we cannot rely on the default value
out 0x92, al
jmp .check
.ps2_enable_wait_in: ;;Routine for .ps2_enable
in al, 0x64
test al, 2
jnz .ps2_enable_wait_in
ret
.ps2_enable_wait_out: ;;Routine for .ps2_enable
in al, 0x64
test al, 1
jz .ps2_enable_wait_out
ret
.end:
call ok

get_memory_map:
mov ecx, BIOS_MMAP_STR
call print_string
;;Prepare parameters
mov eax, 0xE820
mov ebx, 0
mov di, 0x7E00
mov ecx, 24
mov edx, "PAMS" ;;SMAP signature backwards (because endianess)
int 0x15
jc fail ;;Unsupported BIOS version
cmp eax, "PAMS"
jne fail ;;BIOS did not set EAX to the SMAP signature
.bios_call:
mov [di + 20], dword 1 ;;Force valid ACPI 3.X entry
add di, 24 
cmp di, 0x8000
jae fail ;;Too many entries
mov edx, "PAMS" ;;May be cleared by some BIOSes
mov eax, 0xE820
mov ecx, 24
int 0x15
jc .end ;;The previous entry is the final entry
cmp ebx, 0
je .final_curr_entry ;;The current entry is the final entry
jmp .bios_call
.final_curr_entry:
add di, 24
.end:
mov [di + 16], dword 0x55AA ;;End signature for later processing
call ok
hlt

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

OK_STR: db "OK", 0xA, 0xD, "-> ", 0
FAIL_STR: db "FAIL", 0
ENABLE_A20_STR: db "Enabling A20 line...", 0
BIOS_MMAP_STR: db "Retrieving BIOS E820 memory map...", 0
times (5 * 512) - ($-$$) db 0

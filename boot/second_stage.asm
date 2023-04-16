[BITS 16]
[ORG 0x8000]

;;The second stage:
;;1. Retrieves needed information for the kernel
;;2. Loads kernel
;;3. Enters the best avaliable video mode
;;4. Enters protected mode
;;5. Jumps to kernel

;;Disk drive # is passed on stack
;;ESI contains booted entry ptr
cli
push dword [esi + 8] ;;Save booted entry LBA
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
mov di, 0x1000
mov ecx, 24
mov edx, "PAMS" ;;SMAP signature backwards (because endianess)
int 0x15
jc fail ;;Unsupported BIOS version
cmp eax, "PAMS"
jne fail ;;BIOS did not set EAX to the SMAP signature
.bios_call:
mov [di + 20], dword 1 ;;Force valid ACPI 3.X entry
add di, 24 
cmp di, 0x2000
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
mov [di], dword 0x55AA ;;End signature for later processing
call ok

load_kernel:
mov ecx, LOADING_KERNEL_STR
call print_string
lgdt[gdtr]
.load_bpb:
;;Loads the VBR
mov eax, [ebp - 6]
mov [DAP.lba], eax
mov dx, [ebp - 2]
mov ah, 0x42
mov esi, DAP
int 0x13
;;Copies the BPB portion into a dummy BPB contained in this file
mov esi, 0x7C00
mov edi, bpb
mov ecx, 90/2
rep movsw
.load_root_directory:
;;Loads root directory cluster
mov eax, [ebp - 6]
add eax, [bpb.FATz32]
add eax, [bpb.RootClus]
sub eax, 2 ;;2 reserved FAT entries
xor ebx, ebx
mov bx, [bpb.RsvdSecCnt]
add eax, ebx
mov [DAP.lba], eax
mov dx, [ebp - 2]
mov ah, 0x42
mov esi, DAP
int 0x13
.find_kernel:
;;Parses root directory cluster to find KERNEL.BIN's cluster #
mov eax, 0x7C00
.skip_lfn_entries:
cmp eax, 0x7E00 ;;Make sure we haven't reached the end of the root directory's first cluster (assume KERNEL.BIN is the root directory's first cluster)
je fail
cmp [eax + 11], byte 0x0F
jne .check_filename_1
add eax, 32
jmp .skip_lfn_entries
.check_filename_1:
cmp [eax], dword "KERN"
je .check_filename_2
add eax, 32
jmp .skip_lfn_entries
.check_filename_2:
cmp [eax + 4], word "EL"
je .check_extention
add eax, 32
jmp .skip_lfn_entries
.check_extention:
mov ebx, [eax + 8]
shl ebx, 8
cmp ebx, 0x4E494200 ;;Hex for 'BIN' followed by 0x00
je .parse_memory_map
add eax, 32
jmp .skip_lfn_entries
.parse_memory_map:
;;Finds a contigous area of memory to load kernel into
mov ecx, [eax + 28] ;;File (kernel) size
mov ebx, 0x1000 - 24
.check_memory_region:
add ebx, 24
cmp [ebx], word 0x55AA ;;Check if the table has ended
je fail
cmp [ebx + 16], dword 1 ;;Check if region is usable
jne .check_memory_region
cmp [ebx + 4], dword 0 ;;Make sure we can address the region (upper 32 bits should be 0 so we know its located <4GB 32bit limit)
jne .check_memory_region
cmp [ebx + 12], dword 0 ;;If this region is >4GB, we know it is big enough for the kernel
jne .found_memory_region
cmp [ebx + 8], ecx ;;Check if region has enough memory for the kernel
jb .check_memory_region
.found_memory_region:
mov edx, [ebx]
push edx ;;Copy destination
push edx ;;Kernel location
.get_starting_cluster:
mov bx, [eax + 20] ;;High word of cluster #
shl ebx, 16
mov bx, [eax + 26] ;;Low word of cluster #
push ebx
.load_cluster:
mov eax, [bpb.FATz32]
add eax, ebx
sub eax, 2 ;;2 reserved FAT entries
xor ecx, ecx
mov cx, [bpb.RsvdSecCnt]
add eax, ecx
add eax, [ebp - 6]
mov [DAP.lba], eax
mov dx, [ebp - 2]
mov ah, 0x42
mov esi, DAP
int 0x13
mov edx, [ebp - 10]
.prepare_copy:
;;Enter PMode
cli
mov eax, cr0
or al, 1
mov cr0, eax
jmp 0x8:.copy_to_hi_mem
.copy_to_hi_mem:
;;Use rep movsd to copy the cluster from 0x7C00 to the kernel location
[BITS 32]
mov ax, 0x10
mov es, ax
mov ds, ax
cld
mov esi, 0x7C00
mov edi, edx
mov ecx, 512/4
rep movsd
jmp 0x18:.finish_copy
.finish_copy:
;;Exit PMode
[BITS 16]
mov ax, 0x20
mov es, ax
mov ds, ax
mov eax, cr0
and al, 0xFE
mov cr0, eax
jmp 0:.get_next_cluster
.get_next_cluster:
mov ax, 0
mov es, ax
mov ds, ax
add edx, 512
mov [ebp - 10], edx
;;Loads the FAT entry and gets the next cluster
pop ebx
mov eax, ebx
mov ebx, 128 ;;128 FAT entries per sector
xor edx, edx
div ebx ;;EAX=FAT sector, EDX=entry # in that FAT sector
push edx
add eax, [ebp - 6]
xor ebx, ebx
mov bx, [bpb.RsvdSecCnt]
add eax, ebx
mov [DAP.lba], eax
mov dx, [ebp - 2]
mov ah, 0x42
mov esi, DAP
int 0x13
;;Get offset in FAT sector to find the entry
pop edx
mov eax, edx
xor edx, edx
mov ecx, 4
mul ecx
add eax, 0x7C00
mov ebx, [eax]
push ebx
cmp ebx, 0x0FFFFFF8 ;;Check if this was the final cluster in chain
jb .load_cluster
pop eax ;;Discard EBX we just pushed
pop edx
pop eax ;;Discard EDX we used earlier (copy destination)
push edx;;Save kernel location
call ok

;;Test to make sure kernel is OK
pop edx
mov eax, cr0
or eax, 1
mov cr0, eax
jmp 0x8:.jmp
[BITS 32]
.jmp: jmp edx
[BITS 16]

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

;;Structures
align 4
DAP:
.size: db 0x10
.unused: db 0
.count: dw 1
.offset: dw 0x7C00
.segment: dw 0
.lba: dq 0 

gdt:
.start:
.null_segment: dq 0
.code_limit_low: dw 0xFFFF 
.code_base_low: dw 0
.code_base_mid: db 0
.code_access: db 0b10011110
.code_limit_high: db 0b11001111
.code_base_high: db 0
.data_limit_low: dw 0xFFFF 
.data_base_low: dw 0
.data_base_mid: db 0
.data_access: db 0b10010010
.data_limit_high: db 0b11001111
.data_base_high: db 0
.code16_limit_low: dw 0xFFFF 
.code16_base_low: dw 0
.code16_base_mid: db 0
.code16_access: db 0b10011110
.code16_limit_high: db 0b00001111
.code16_base_high: db 0
.data16_limit_low: dw 0xFFFF 
.data16_base_low: dw 0
.data16_base_mid: db 0
.data16_access: db 0b10010010
.data16_limit_high: db 0b00001111
.data16_base_high: db 0
.end:

gdtr:
.size: dw gdt.end - gdt.start - 1
.offset: dd gdt

%include "boot/vbr/bpb.asm"

;;Strings
OK_STR: db "OK", 0xA, 0xD, "-> ", 0
FAIL_STR: db "FAIL", 0
ENABLE_A20_STR: db "Enabling A20 line...", 0
BIOS_MMAP_STR: db "Retrieving BIOS E820 memory map...", 0
LOADING_KERNEL_STR: db "Loading kernel...", 0
times (5 * 512) - ($-$$) db 0

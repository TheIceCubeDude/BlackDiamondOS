[BITS 16]
[ORG 0x7E00]

;;The second stage:
;;1. Retrieves needed information for the kernel
;;2. Loads kernel
;;3. Enters the best avaliable video mode
;;4. Enters protected mode
;;5. Jumps to kernel

;;Disk drive # is passed on stack
;;Booted LBA is passed on stack
cli
call ok
xor cx, cx
xor edx, edx

check_system_support:
mov ecx, SYS_SUPPORT_STR
call print_string
.cpuid:
pushfd ;;Get EFLAGS
pop eax
mov ebx, eax ;;Save EFLAGS
xor eax, 0x00200000 ;;Flip ID bit
push eax
popfd ;;Set EFLAGS to modified value
pushfd ;;Get EFLAGS again
pop eax
push ebx ;;Restore EFLAGS
popfd
cmp eax, ebx ;;Check if the bit was flipped
je fail
.lmode:
mov eax, 0x80000000
cpuid ;;Check if long mode check function exists
cmp eax, 0x80000001
jb fail
mov eax, 0x80000001
cpuid ;;Check for long mode with the function (bit 29 indicates support)
test edx, 0x20000000
jz fail
.sse2:
mov eax, 1
cpuid
test edx, 0x4000000 ;;Check for SSE2 (bit 26 indicates support) (SSE1 does not support writing double quadwords to memory)
jz fail
;;Enable SSE
mov eax, cr0
and ax, 0xFFFB ;;Clear FPU coprocessor emulation
or ax, 2 ;;Set FPU coprocessor monitoring
mov cr0, eax
mov eax, cr4
or ax, 0x600 ;;Set SSE enable and SSE exception unmask
mov cr4, eax
.success:
call ok

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
mov [DAP.offset], word 0x2000
mov [DAP.count], word 1
mov dx, [ebp - 2]
mov ah, 0x42
mov esi, DAP
int 0x13
;;Copies the BPB portion into a dummy BPB contained in this file
mov esi, 0x2000
mov edi, bpb
mov ecx, 90/2
rep movsw
.find_file:
mov eax, "KERN"
mov ebx, "EL  "
call find_file_in_root_dir
push ebx
.parse_memory_map:
;;Finds a contigous area of memory to load kernel into
mov ebx, 0x1000 - 24
.check_memory_region:
add ebx, 24
cmp [ebx], word 0x55AA ;;Check if the table has ended
je fail
cmp [ebx + 16], dword 1 ;;Check if region is usable
jne .check_memory_region
cmp [ebx + 4], dword 0 ;;Make sure this region is located <2GB, so we can use REL32 jmps and calls, and RIP relative addressing 
jne .check_memory_region
cmp [ebx + 8], dword 2147483648
ja .check_memory_region
cmp [ebx + 12], dword 0 ;;If this region has a size >4GB, we know it is big enough
jne .found_memory_region
cmp [ebx + 8], eax ;;Check if this region has a size big enough for at least the kernel
jb .check_memory_region
.found_memory_region:
mov edx, [ebx]
pop ebx ;;Get starting cluster #
push edx ;;Copy destination
push edx ;;Kernel location
.load_cluster:
mov cx, 0x2000
call load_cluster
mov edx, [ebp - 10]
.prepare_copy:
;;Enter PMode
cli
mov eax, cr0
or al, 1
mov cr0, eax
jmp 0x8:.copy_to_hi_mem
.copy_to_hi_mem:
;;Use rep movsd to copy the cluster from 0x2000 to the kernel location
[BITS 32]
mov ax, 0x10
mov es, ax
mov ds, ax
cld
mov esi, 0x2000
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
xor eax, eax
mov al, [bpb.SecPerClus]
mov ecx, 512
mul ecx
add edx, eax
mov [ebp - 10], edx
;;Loads the FAT entry and gets the next cluster
push cx
call load_fat_entry
pop cx
cmp ebx, 0x0FFFFFF8 ;;Check if this was the final cluster in chain
jb .load_cluster
pop edx
pop eax ;;Discard EDX we used earlier (copy destination)
push edx;;Save kernel location
call ok
jmp get_vbe_modes

get_vbe_modes:
;;Get the highest resolution VBE mode with 32 bits per pixel
mov ecx, GET_VIDEO_MODE_STR
call print_string
.get_controller_info:
mov ax, 0x4F00
mov di, vbe_controller_info
int 0x10
cmp ax, 0x004F
jne fail
mov bx, [vbe_controller_info.VideoModePtr] ;;Ptr offset to a list of video modes
mov ax, [vbe_controller_info.VideoModePtr + 2] ;;Ptr segment to a list of video modes
mov fs, ax ;;ES is used by the function calls
xor edx, edx
mov ax, 0xFFFF
mov gs, ax ;;Video mode can never be -1, so use that as an empty value
mov ax, 0xBEEF
.goto_next_mode:
cmp ax, 0xBEEF ;;Don't increment counter if this is first iteration of the loop
je .get_mode_info
add bx, 2
jnc .get_mode_info ;;If the addition overflows past 16 bits, increase es register
mov ax, fs
add ax, 4096
mov fs, ax
mov bx, 0
.get_mode_info:
mov cx, [fs:bx] 
cmp cx, 0xFFFF ;;End of list
je .end
mov ax, 0x4F01
mov di, vbe_mode_info
int 0x10
cmp ax, 0x004F
jne .goto_next_mode ;;Most likely: mode isn't avaliable so do not give up
.check_mode_info:
cmp [vbe_mode_info.BitsPerPixel], byte 32 ;;Make sure it is our desired BPP
;;TODO: check MemoryModel, masks and field positions, linear support (anything else?)
jne .goto_next_mode
.check_mode_res:
push edx
push cx
xor eax, eax
xor ecx, ecx
xor edx, edx
mov ax, word [vbe_mode_info.XResolution]
mov cx, word [vbe_mode_info.YResolution]
mul ecx
pop cx
pop edx
cmp eax, edx ;;Select this resolution if it is highest than the previously selected one
jb .goto_next_mode
mov edx, eax
mov gs, cx ;;Also store the video mode ID in GS (we are treating GS as general purpose) 
jmp .goto_next_mode
.end:
mov ax, gs
cmp ax, 0xFFFF
je fail
call ok

set_vbe_mode:
mov ecx, GET_VIDEO_MODE_STR
call print_string
;;Get the mode info for the selected mode for the kernel's use, and sets the selected mode
.get_mode_info:
mov cx, gs
mov ax, 0x4F01
mov di, vbe_mode_info
int 0x10
.set_mode:
mov ax, 0x4F02
mov bx, cx
int 0x10
cmp ax, 0x004F
jne fail

enter_lmode:
pop edx ;;Kernel location
cli
hlt

;;Useful subroutines
fail:
mov ecx, FAIL_STR
call print_string
hlt

ok:
mov ecx, OK_STR
call print_string
ret

%include "boot/util/misc.asm"
%include "boot/util/fat32.asm"

;;Data structures
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

vbe_controller_info:
.VBESignature: dd "VBE2"
.VBEVersion: dw 0
.OemStringPtr: dd 0
.Capabilities: times 4 db 0
.VideoModePtr: dd 0
.TotalMemory: dw 0
.OemSoftwareRev: dw 0
.OemVendorNamePtr: dd 0
.OemProductNamePtr: dd 0
.OemProductRevPtr: dd 0
.Reserved: times 222 db 0
.OemData: times 256 db 0

vbe_mode_info:
.ModeAttributes: dw 0
.WinAAttributes: db 0
.WinBAttributes: db 0
.WinGranularity: dw 0
.WinSize: dw 0
.WinASegment: dw 0
.WinBSegment: dw 0
.WinFuncPtr: dd 0
.BytesPerScanLine: dw 0
.XResolution: dw 0
.YResolution: dw 0
.XCharSize: db 0
.YCharSize: db 0
.NumberOfPlanes: db 0
.BitsPerPixel: db 0
.NumberOfBanks: db 0
.MemoryModel: db 0
.BankSize: db 0
.NumberOfImagePages: db 0
.Reserved0: db 0
.RedMaskSize: db 0
.RedFieldPosition: db 0
.GreenMaskSize: db 0
.GreenFieldPosition: db 0
.BlueMaskSize: db 0
.BlueFieldPosition: db 0
.RsvdMaskSize: db 0
.RsvdFieldPosition: db 0
.DirectColorModeInfo: db 0
.PhysBasePtr: dd 0
.Reserved1: dd 0
.Reserved2: dd 0
.LinBytesPerScanLine dw 0
.BnkNumberOfImagePages db 0 
.LinNumberOfImagePages db 0
.LinRedMaskSize db 0
.LinRedFieldPosition db  0
.LinGreenMaskSize db 0
.LinGreenFieldPosition db 0
.LinBlueMaskSize db 0
.LinBlueFieldPosition db 0
.LinRsvdMaskSize db 0
.LinRsvdFieldPosition db 0
.MaxPixelClock dd 0
.Reserved3: times 189 db 0

%include "boot/vbr/bpb.asm"

;;Strings
OK_STR: db "OK", 0xA, 0xD, "-> ", 0
FAIL_STR: db "FAIL", 0
ENABLE_A20_STR: db "Enabling A20 line...", 0
SYS_SUPPORT_STR: db "Checking for system support (64 bits + SSE2)...", 0
BIOS_MMAP_STR: db "Retrieving BIOS E820 memory map...", 0
LOADING_KERNEL_STR: db "Loading kernel...", 0
GET_VIDEO_MODE_STR: db "Finding compatible VBE video mode...", 0
SET_VIDEO_MODE_STR: db "Setting video mode and executing kernel...", 0
times (5 * 512) - ($-$$) db 0

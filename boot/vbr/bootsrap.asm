;;The VBR loads the 2nd stage bootloader
;;The MBR should have passed a pointer to the partition table entry that was booted in DS:ESI, but we assume DS is 0
;;The MBR should have forwarded the boot drive in DL
cli
;;Setup stack (MBR should have set up segments)
mov esp, 0x1000 ;;Stack grows downwards
mov ebp, esp
xor eax, eax
mov ss, ax
push dx
push dword [esi + 8] ;;Save booted LBA
;;Check FAT32 filesys
cmp [bpb.SecPerClus], byte 32
ja fail
;;Print message
mov ecx, BOOT_STR
call print_string

find_2nd_stage:
mov eax, "BOOT"
mov ebx, "    "
call find_file_in_root_dir
cmp eax, 0xFFFF
jg fail
mov cx, 0x7E00

load_next_cluster:
call load_cluster
xor ax, ax
xor dx, dx
mov al, [bpb.SecPerClus]
push bx
mov bx, 512
mul bx
pop bx
add cx, ax

get_next_cluster:
push cx
call load_fat_entry
pop cx
cmp ebx, 0x0FFFFFF8
jb load_next_cluster

exec_2nd_stage:
;;Disk drive # is passed on stack
;;ESI contains booted entry ptr
jmp 0x7E00

fail: hlt
%include "boot/util/misc.asm"
%include "boot/util/fat32.asm"

;BOOT_STR: db 0xA, 0xA, 0xA, 0xD, "-> BlackDiamondOS Bootloader <-", 
BOOT_STR: db "Ldng...", 0
times 510-($-$$) db 0
dw 0xAA55

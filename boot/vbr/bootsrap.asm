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
mov ecx, BOOT_STR
call print_string

load_root_directory:
;;Loads root directory cluster
push esi
mov eax, [bpb.FATz32]
add eax, [bpb.RootClus] 
sub eax, 2 ;;2 reserved FAT entries
xor ebx, ebx
mov bx, [bpb.RsvdSecCnt]
add eax, ebx
add eax, [esi + 8]
mov [DAP.lba], eax
mov dx, [ebp - 2]
mov ah, 0x42
mov esi, DAP
int 0x13
pop esi

find_2nd_stage:
;;Parses root directory cluster to find BOOT.BIN's cluster #
mov eax, 0x7E00
.skip_lfn_entries:
cmp eax, 0x8000 ;;Make sure we haven't reached the end of the root directory's first cluster (assume BOOT.BIN is in the root direcotry's first cluster)
je halt
cmp [eax + 11], byte 0x0F
jne .check_filename
add eax, 32
jmp .skip_lfn_entries
.check_filename:
cmp [eax], dword "BOOT"
je .check_extention
add eax, 32
jmp .skip_lfn_entries
.check_extention:
mov ebx, [eax + 8]
shl ebx, 8
cmp ebx, 0x4E494200 ;;Hex for BIN followed by 0x00
je .get_cluster
add eax, 32
jmp .skip_lfn_entries
.get_cluster:
mov bx, [eax + 20] ;;High word of cluster #
shl ebx, 16
mov bx, [eax + 26] ;;Low word of cluster #

load_cluster:
;;Load the cluster in ebx
push esi
push ebx
mov eax, [bpb.FATz32]
add eax, ebx
sub eax, 2 ;;2 reserved FAT entries
xor ecx, ecx
mov cx, [bpb.RsvdSecCnt]
add eax, ecx
add eax, [esi + 8]
mov [DAP.lba], eax
add [DAP.offset], word 512
mov dx, [ebp - 2]
mov ah, 0x42
mov esi, DAP
int 0x13
pop ebx
pop esi

get_next_cluster:
;;Loads the FAT entry and gets the next cluster
push esi
push word [DAP.offset]
mov eax, ebx
mov ebx, 128 ;;128 FAT entries per sector
xor edx, edx
div ebx ;;EAX=FAT sector, EDX=entry # in that FAT sector
push edx
add eax, [esi + 8]
xor ebx, ebx
mov bx, [bpb.RsvdSecCnt]
add eax, ebx
mov [DAP.lba], eax
mov [DAP.offset], word 0x7E00
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
add eax, 0x7E00
mov ebx, [eax]
pop word [DAP.offset]
pop esi
cmp ebx, 0x0FFFFFF8 ;;Check if this was the final cluster in chain
jb load_cluster

exec_2nd_stage:
;;Disk drive # is passed on stack
;;ESI contains booted entry ptr
jmp 0x8000

halt:
hlt

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

align 4
DAP:
.size: db 0x10
.unused: db 0
.count: dw 1
.offset: dw 0x7E00
.segment: dw 0
.lba: dq 0

BOOT_STR: db 0xA, 0xA, 0xA, 0xD, "-> BlackDiamondOS Bootloader <-", 0xA, 0xA, 0xD, "-> Loading 2nd stage...", 0
times 510-($-$$) db 0
dw 0xAA55

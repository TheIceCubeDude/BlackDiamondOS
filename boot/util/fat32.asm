find_file_in_root_dir:
;;EAX = first half of filename
;;EBX = second half of filename
;;Returns file size in EAX
;;Returns sector # in EBX
push ebx
push eax
.load_root_directory:
mov eax, [bpb.RootClus]
sub eax, 2 ;;2 reserved FAT entries
add eax, [bpb.FATz32]
xor bx, bx
mov bl, [bpb.SecPerClus]
mov cx, 0x2000
call load_fs_sectors
.find_file:
;;Parses root directory cluster to find file's cluster #
xor ax, ax
xor dx, dx
mov al, [bpb.SecPerClus]
mov bx, 512
mul bx
add ax, 0x2000
mov cx, ax
mov eax, 0x2000
.skip_lfn_entries:
cmp ax, cx ;;Make sure we haven't reached the end of the root directory's first cluster (assume the file is in the root directory's first cluster)
je fail
cmp [eax + 11], byte 0x0F
jne .check_filename
add ax, 32
jmp .skip_lfn_entries
.check_filename:
mov ebx, [esp]
cmp [eax], ebx
je .check_filename_2
add ax, 32
jmp .skip_lfn_entries
.check_filename_2:
mov ebx, [esp + 4]
cmp [eax + 4], ebx
je .check_extention
add ax, 32
jmp .skip_lfn_entries
.check_extention:
mov ebx, [eax + 8]
shl ebx, 8
cmp ebx, 0x4E494200 ;;Hex for "BIN" followed by 0x00
je .get_info
add ax, 32
jmp .skip_lfn_entries
.get_info:
pop ebx
pop ebx
mov bx, [eax + 20] ;;High word of cluster #
shl ebx, 16
mov bx, [eax + 26] ;;Low word of cluster #
mov eax, [eax + 28] ;;File size
ret

load_fs_sectors:
;;EAX = starting sector
;;BX = # of sectors
;;CX = address
mov [DAP.count], bx
mov [DAP.offset], cx
xor ebx, ebx
mov bx, [bpb.RsvdSecCnt]
add eax, ebx
add eax, [ebp - 6]
mov [DAP.lba], eax
mov dx, [ebp - 2]
mov ah, 0x42
mov esi, DAP
int 0x13
ret

load_cluster:
;;EBX = cluster #
;;CX = location
push ebx
mov eax, ebx
sub eax, 2 ;;2 reserved FAT entries
xor edx, edx
xor ebx, ebx
mov bl, [bpb.SecPerClus]
mul ebx
add eax, [bpb.FATz32]
xor bx, bx
mov bl, [bpb.SecPerClus]
call load_fs_sectors
pop ebx
ret

load_fat_entry:
;;TODO:this function trashes stack???
;;EBX = FAT entry
;;Returns next FAT entry in EBX
mov eax, ebx
mov ebx, 128 ;;128 FAT entries per sector
xor edx, edx
div ebx ;;EAX=FAT sector, EDX=entry # in that sector
mov bx, 1
mov cx, 0x2000
push dx
call load_fs_sectors
pop dx
mov eax, edx
xor edx, edx
mov ebx, 4
mul ebx ;;Get offset of entry
mov ebx, [eax + 0x2000]
ret

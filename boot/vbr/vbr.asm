;;This is formatted as a FAT32 filesystem
;;The second stage is located in the reserved sectors
;;The kernel is a file on the filesystem
[BITS 16]
[ORG 0x7C00]

;;Sector #0
%include "boot/vbr/bpb.asm"
%include "boot/vbr/bootsrap.asm"

;;Sector #1
%include "boot/vbr/fs_info.asm"

;;Sectors #2, #3, #4, #5
times 4*512 db 0

;;Sector #6
times 512 db 0 ;;Fake backup BPB 

;;Rest of reserved sectors
times 25*512 db 0

;;First 2 reseved FAT entries (when reading Microsoft's spec, make sure to check how many digits are in the hex values!!!)
dd 0xFFFFFFF8 ;;Set to media type, with rest of bits 1
dd 0xFFFFFFFF ;;Set to end of cluser value
;;First FAT entry
dd 0xFFFFFFFF ;;End of cluster



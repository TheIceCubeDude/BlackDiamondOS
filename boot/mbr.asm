[BITS 16]
[ORG 0x600]

%include "boot/mbr/bootstrap.asm"

times 446-($-$$) db 0
%include "boot/mbr/partition_table.asm"
dw 0xAA55

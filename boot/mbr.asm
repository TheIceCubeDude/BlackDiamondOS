[BITS 16]
[ORG 0x600]

%include "boot/bootstrap.asm"

times 446-($-$$) db 0
%include "boot/partition_table.asm"
dw 0xAA55

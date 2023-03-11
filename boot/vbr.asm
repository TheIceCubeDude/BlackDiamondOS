[BITS 16]
[ORG 0x7C00]

cli
hlt

times 1024*1024 - ($-$$) db 0

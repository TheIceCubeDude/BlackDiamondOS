attrib_bootable equ 10000000b
attrib		equ 00000000b
OSID		equ 0x8A

partition_1:
	.attributes:		db attrib_bootable
	.startingHead:		db 0
	.startingSector:	db 0
	.startingCylinder: 	db 0
	.systemID:		db OSID
	.endingHead:		db 0
	.endingSector:		db 0
	.endingCylinder: 	db 0
	.lba:			dd 1
	.size:			dd 1

partition_2: times 16 db 0
partition_3: times 16 db 0
partition_4: times 16 db 0

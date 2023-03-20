attrib_active	equ 10000000b
attrib		equ 00000000b
fat32_ID	equ 0x0C

partition_1:
	.attributes:		db attrib_active
	.startingHead:		db 0
	.startingSector:	db 0
	.startingCylinder: 	db 0
	.systemID:		db fat32_ID
	.endingHead:		db 0
	.endingSector:		db 0
	.endingCylinder: 	db 0
	.lba:			dd 1
	.size:			dd (1048576*32)/512

partition_2: times 16 db 0
partition_3: times 16 db 0
partition_4: times 16 db 0

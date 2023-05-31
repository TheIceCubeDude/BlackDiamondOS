%define VOLUME_SIZE 1048576 * 32 ;;32 Megs, make sure to also change partition 1 size 

bpb:
.jmpBoot:
jmp short start
nop
.OEMName: db "MSWIN4.1"
.BytsPerSec: dw 512
.SecPerClus: db 4
.RsvdSecCnt: dw 32
.NumFats: db 1
.RootEntCnt: dw 0
.TotSec16: dw 0
.Media: db 0xF8
.FatSz16: dw 0
.SecPerTrk: dw 63
.NumHeads: dw 255
.HiddSec: dd 0 ;;(AKA starting LBA, but we don't care about this)
.TotSec32: dd VOLUME_SIZE/512 ;;(AKA size in sectors)
.FATz32: dd ((VOLUME_SIZE/512)*4)/512 ;;(AKA FAT size in sectors)
.ExtFlags: dw 0b0000000100000000 ;;Only one active FAT
.FSVer: dw 0
.RootClus: dd 2
.FSInfo: dw 1
.BkBootSec: dw 6
.Reserved: times 12 db 0
.DrvNum: db 0x80
.Reserved1: db 0
.BootSig: db 0x29
.VolID: dd 0xBD05
.VolLab: db "BlK DMND OS"
.FilSysType: db "FAT32   "

start:

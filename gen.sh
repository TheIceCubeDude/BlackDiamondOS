nasm -f bin -o build/mbr.bin boot/mbr.asm
nasm -f bin -o build/vbr.bin boot/vbr.asm

cat build/mbr.bin build/vbr.bin > OS.bin
bochs

alias sudo="doas" # I use doas rather than sudo

nasm -f bin -o build/mbr.bin boot/mbr.asm
nasm -f bin -o build/vbr.bin boot/vbr.asm
nasm -f bin -o build/BOOT.BIN boot/second_stage.asm

rm OS_volume_image.img
truncate --size 32M OS_volume_image.img
dd if=build/vbr.bin of=OS_volume_image.img conv=notrunc
sudo mount OS_volume_image.img mnt
sudo cp build/BOOT.BIN mnt
sudo umount mnt

cat build/mbr.bin OS_volume_image.img > OS_disk_image.img
bochs

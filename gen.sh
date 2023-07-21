alias sudo="doas" # I use doas rather than sudo

nasm -f bin -o build/mbr.bin boot/mbr/mbr.asm
nasm -f bin -o build/vbr.bin boot/vbr/vbr.asm
nasm -f bin -o build/BOOT.BIN boot/second_stage.asm
nasm -f elf64 -o build/kernel/start.elf kernel/core/start.asm

x86_64-elf-gcc -ffreestanding -fPIE -c -o build/kernel/kernel.elf kernel/core/kernel.c

x86_64-elf-gcc -mno-red-zone -ffreestanding -nostdlib -lgcc -T script.ld -o build/kernel/linkedkernel.elf build/kernel/start.elf build/kernel/kernel.elf
x86_64-elf-objcopy -O binary build/kernel/linkedkernel.elf build/KERNEL.BIN

rm OS_volume_image.img
truncate --size 32M OS_volume_image.img
dd if=build/vbr.bin of=OS_volume_image.img conv=notrunc
sudo mount OS_volume_image.img mnt
sudo cp build/BOOT.BIN mnt
sudo cp build/KERNEL.BIN mnt
sudo umount mnt

cat build/mbr.bin OS_volume_image.img > OS_disk_image.img
#./bochs
qemu-system-x86_64 -hda OS_disk_image.img -serial stdio -m 4G

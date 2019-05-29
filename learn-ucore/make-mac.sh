
# rm -rf obj1;
# mkdir obj1;

# i386-elf-gcc -Iboot/ -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Os -nostdinc -c boot/bootasm.S -o obj1/bootasm.o
# i386-elf-gcc -Iboot/ -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Os -nostdinc -c boot/bootmain.c -o obj1/bootmain.o
# i386-elf-ld -m elf_i386 -nostdlib -N -T tools/boot.ld obj1/bootasm.o obj1/bootmain.o -o obj1/bootblock.elf
# i386-elf-objdump -d obj1/bootblock.elf > obj1/bootblock.asm;
# i386-elf-objcopy -S -O binary obj1/bootblock.elf obj1/bootblock.out;

# gcc -o2 -o tools/sign tools/sign.c;
# tools/sign obj1/bootblock.out obj1/bootblock;

# rm -rf disk.img;
# dd if=/dev/zero of=disk.img count=10000
# dd if=obj1/bootblock of=disk.img conv=notrunc
# dd if=bin/kernel of=disk.img seek=1 conv=notrunc

# 下面2条指令启动 qemu 效果是一样的，挂载 3 个分区
# qemu-system-i386 -S -s -parallel stdio -m 512M -hda bin/ucore.img -drive file=bin/swap.img -drive file=bin/sfs.img
# qemu-system-i386 -S -s -parallel stdio -m 512M -drive file=bin/ucore.img -drive file=bin/swap.img -drive file=bin/sfs.img
# gdb -q -tui -x tools/gdbinit

# 下面的指令，可以挂载 4 个分区，qemu 最大也只能挂载 4 个
# qemu-system-i386 -S -s -parallel stdio -m 512M -drive file=bin/ucore.img -drive file=bin/swap.img -drive file=bin/sfs.img -drive file=bin/sfs1.img

make clean;make;
cp bin/sfs.img bin/sfs1.img;




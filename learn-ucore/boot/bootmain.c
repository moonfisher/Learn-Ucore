#include "defs.h"
#include "x86.h"
#include "elf.h"

/* *********************************************************************
 * This a dirt simple boot loader, whose sole job is to boot
 * an ELF kernel image from the first IDE hard disk.
 *
 * DISK LAYOUT
 *  * This program(bootasm.S and bootmain.c) is the bootloader.
 *    It should be stored in the first sector of the disk.
 *
 *  * The 2nd sector onward holds the kernel image.
 *
 *  * The kernel image must be in ELF format.
 *
 * BOOT UP STEPS
 *  * when the CPU boots it loads the BIOS into memory and executes it
 *
 *  * the BIOS intializes devices, sets of the interrupt routines, and
 *    reads the first sector of the boot device(e.g., hard-drive)
 *    into memory and jumps to it.
 *
 *  * Assuming this boot loader is stored in the first sector of the
 *    hard-drive, this code takes over...
 *
 *  * control starts in bootasm.S -- which sets up protected mode,
 *    and a stack so C code then run, then calls bootmain()
 *
 *  * bootmain() in this file takes over, reads in the kernel and jumps to it.
 * */

#define SECTSIZE        512

/* waitdisk - wait for disk ready */
static void waitdisk(void)
{
    while ((inb(0x1F7) & 0xC0) != 0x40)
        /* do nothing */;
}

/* readsect - read a single sector at @secno into @dst */
// 端口地址参考 cat /proc/ioports
static void readsect(void *dst, uint32_t secno)
{
    // wait for disk to be ready
    waitdisk();

    outb(0x1F2, 1);                         // count = 1
    outb(0x1F3, secno & 0xFF);
    outb(0x1F4, (secno >> 8) & 0xFF);
    outb(0x1F5, (secno >> 16) & 0xFF);
    outb(0x1F6, ((secno >> 24) & 0xF) | 0xE0);
    outb(0x1F7, 0x20);                      // cmd 0x20 - read sectors

    // wait for disk to be ready
    waitdisk();

    // read a sector
    insl(0x1F0, dst, SECTSIZE / 4);
}

/* *
 * readseg - read @count bytes at @offset from kernel into virtual address @va,
 * might copy more than asked.
 * */
static void readseg(uintptr_t va, uint32_t count, uint32_t offset)
{
    uintptr_t end_va = va + count;

    // round down to sector boundary
    va -= offset % SECTSIZE;

    // translate from bytes to sectors; kernel starts at sector 1
    uint32_t secno = (offset / SECTSIZE) + 1;

    // If this is too slow, we could read lots of sectors at a time.
    // We'd write more to memory than asked, but it doesn't matter --
    // we load in increasing order.
    for (; va < end_va; va += SECTSIZE, secno ++)
    {
        readsect((void *)va, secno);
    }
}

/* bootmain - the entry of bootloader */
/*
 Bootloader 加载内核就要读取文件，在实模式下可以用 BIOS 的 INT 13h 中断。
 内核文件放在哪里，怎么查找读取，这里牵涉到文件系统，Bootloader 要从硬盘（软盘）的文件
 系统中查找内核文件，因此 Bootloader 需要解析文件系统的能力。

 对于 ucore 简单操作系统来说，可以简单处理，把内核文件放到 Bootloader 之后，即从软盘的
 第 1 个扇区开始，这样我们可以不需要支持文件系统，直接读取扇区数据加载到内存即可。
 
 这里 bootloader 不是通用内核加载器，只能识别 elf 格式的内核
 bootloader 和内核 elf 文件，都在 ucore.img 的文件上，内核 elf 文件位置也不是任意的，
 而是和 bootloader 一起，连续存放，如果 bootloader 想更为通用，要参考 grub 原理
 
 ====== bootloader start ======
 fafc 31c0 8ed8 8ec0 8ed0 e464 a802 75fa
 b0d1 e664 e464 a802 75fa b0df e660 66c7
 0600 8000 0000 0066 31db bf04 8066 b820
 e800 0066 b914 0000 0066 ba50 414d 53cd
 1573 08c7 0600 8039 30eb 0e83 c714 66ff
 0600 8066 83fb 0075 d40f 0116 c47d 0f20
 c066 83c8 010f 22c0 ea6d 7c08 0066 b810
 008e d88e c08e e08e e88e d0bd 0000 0000
 bc00 7c00 00e8 7400 0000 ebfe 5589 e557
 5389 c789 d1bb f701 0000 89da ec83 e0c0
 3c40 75f6 b001 baf2 0100 00ee baf3 0100
 0088 c8ee 89c8 c1e8 08ba f401 0000 ee89
 c8c1 e810 baf5 0100 00ee 89c8 c1e8 1883
 e00f 83c8 e0ba f601 0000 eeb0 2089 daee
 baf7 0100 00ec 83e0 c03c 4075 f8b9 8000
 0000 baf0 0100 00fc f26d 5b5f 5dc3 5589
 e557 5653 83ec 1cbb 0100 0000 89d8 c1e0
 0905 00fe 0000 89da e86f ffff ff43 83fb
 0975 e981 3d00 0001 007f 454c 4675 6aa1
 1c00 0100 8d98 0000 0100 0fb7 052c 0001
 00c1 e005 01d8 8945 e43b 5de4 733f 8b4b
 048b 7308 81e6 ffff ff00 8b43 1401 f089
 45e0 89c8 25ff 0100 0029 c6c1 e909 8d79
 0139 75e0 7612 89fa 89f0 e80d ffff ff81
 c600 0200 0047 ebe9 83c3 20eb bca1 1800
 0100 0500 0000 40ff d0ba 008a ffff 89d0
 66ef b800 8eff ff66 efeb fe00 0000 0000
 0000 0000 ffff 0000 009a cf00 ffff 0000
 0092 cf00 1700 ac7d 0000 0000 0000 0000
 0000 0000 0000 0000 0000 0000 0000 0000
 0000 0000 0000 0000 0000 0000 0000 0000
 0000 0000 0000 0000 0000 0000 0000 55aa
 ====== bootloader end ======
 
 ====== kernel elf start ======
 7f45 4c46 0101 0100 0000 0000 0000 0000
 0200 0300 0100 0000 0000 10c0 3400 0000
 b0b1 1c00 0000 0000 3400 2000 0200 2800
 0c00 0b00 0100 0000 0010 0000 0000 10c0
 0000 10c0 b181 0d00 b181 0d00 0500 0000
 0010 0000 0100 0000 00a0 0d00 0090 1dc0
 0090 1dc0 0050 0e00 70fe 1b00 0600 0000
 ====== kernel elf end ======
 
 ====== kernel code start ======
 b800 d01d 000f 22d8 0f20 c00d 2f00 0580
 83e0 f30f 22c0 8d05 1e00 10c0 ffe0 31c0
 a300 d01d c0bd 0000 0000 bc00 b01d c0e8
 3f00 0000 ebfe 5589 e583 ec10 8b55 088b
 450c 8b4d 08f0 8702 8945 fc8b 45fc c9c3
 5589 e583 ec08 83ec 0c68 a0ba 1dc0 e86d
 
 ......
 
 ====== kernel code end ======
*/
void bootmain(void)
{
    struct elfhdr *elfh = (struct elfhdr *)0x10000;
    
    // read the 1st page off disk 从硬盘上读取内核前 4k 字节到物理地址 0x10000
    // 这里并没有读完整所有内核 elf 文件到内存，只是先把 elf 文件头读出来解析
    readseg((uintptr_t)elfh, SECTSIZE * 8, 0);

    // is this a valid ELF?
    if (elfh->e_magic != ELF_MAGIC)
    {
        goto bad;
    }

    struct proghdr *ph, *eph;

    // load each program segment (ignores ph flags)
    ph = (struct proghdr *)((uintptr_t)elfh + elfh->e_phoff);
    eph = ph + elfh->e_phnum;
    
    /*  编译出来的 kernel 的头信息
     readelf -e bin/kernel
     ELF 头：
     Magic：  7f 45 4c 46 01 01 01 00 00 00 00 00 00 00 00 00
     类别:                              ELF32
     数据:                              2 补码，小端序 (little endian)
     Version:                          1 (current)
     OS/ABI:                           UNIX - System V
     ABI 版本:                          0
     类型:                              EXEC (可执行文件)
     系统架构:                           Intel 80386
     版本:                              0x1
     入口点地址：                         0xc0100000
     程序头起点：                         52 (bytes into file)
     Start of section headers:          1880496 (bytes into file)
     标志：                              0x0
     Size of this header:               52 (bytes)
     Size of program headers:           32 (bytes)
     Number of program headers:         2
     Size of section headers:           40 (bytes)
     Number of section headers:         12
     Section header string table index: 11
     
     节头：
     [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
     [ 0]                   NULL            00000000 000000 000000 00      0   0  0
     [ 1] .text             PROGBITS        c0100000 001000 030108 00  AX  0   0 4096
     [ 2] .rodata           PROGBITS        c0130120 031120 0083b4 00   A  0   0 32
     [ 3] .stab             PROGBITS        c01384d4 0394d4 082d4d 0c   A  4   0  4
     [ 4] .stabstr          STRTAB          c01bb221 0bc221 01cf90 00   A  0   0  1
     [ 5] .data             PROGBITS        c01d9000 0da000 003510 00  WA  0   0 4096
     [ 6] .data.pgdir       PROGBITS        c01dd000 0de000 0e1000 00  WA  0   0 4096
     [ 7] .bss              NOBITS          c02be000 1bf000 0dae70 00  WA  0   0 4096
     [ 8] .comment          PROGBITS        00000000 1bf000 000011 01  MS  0   0  1
     [ 9] .symtab           SYMTAB          00000000 1bf014 007690 10     10 739  4
     [10] .strtab           STRTAB          00000000 1c66a4 004ab4 00      0   0  1
     [11] .shstrtab         STRTAB          00000000 1cb158 000058 00      0   0  1
     Key to Flags:
     W (write), A (alloc), X (execute), M (merge), S (strings), I (info),
     L (link order), O (extra OS processing required), G (group), T (TLS),
     C (compressed), x (unknown), o (OS specific), E (exclude),
     p (processor specific)
     
     程序头：
     Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
     LOAD           0x001000 0xc0100000 0xc0100000 0xd81b1 0xd81b1 R E 0x1000
     LOAD           0x0da000 0xc01d9000 0xc01d9000 0xe5000 0x1bfe70 RW  0x1000
     
     Section to Segment mapping:
     段节...
     00     .text .rodata .stab .stabstr
     01     .data .data.pgdir .bss
    */
    
    // 这个循环会根据 elf 格式，把内核所有代码段，数据段全局加载到内存指定虚拟地址上
    for (; ph < eph; ph ++)
    {
        // 内核代码段根据 elf 里的虚拟地址加载到对应的物理地址上，但此时没有启用分页，先减去 0xC0000000
        // ph->p_va = 0xC0100000, ph->p_va & 0xFFFFFF = 0x00100000
        readseg(ph->p_va & 0xFFFFFF, ph->p_memsz, ph->p_offset);
    }

    // call the entry point from the ELF header
    // note: does not return
    // 找到内核入口地址，虚拟地址 elfh->e_entry = 0xC0100000，此时还没开启分页，实际物理地址 0x00100000
    void (*kern_entry)(void);
    //kern_entry = elfh->e_entry & 0xFFFFFF;
    kern_entry = (void *)(elfh->e_entry - 0xC0000000);
    kern_entry();
    
bad:
    outw(0x8A00, 0x8A00);
    outw(0x8A00, 0x8E00);

    /* do nothing */
    while (1);
}


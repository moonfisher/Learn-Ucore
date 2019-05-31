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
void bootmain(void)
{
    struct elfhdr *elfh = (struct elfhdr *)0x10000;
    
    // read the 1st page off disk 从硬盘上读取内核 4k 字节到物理地址 0x10000
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
    /*  编译出来的kernel的头信息 
     *  $ readelf.exe -l kernel
     *
     *  Elf file type is EXEC (Executable file)
     *  Entry point 0xc0100000
     *  There are 2 program headers, starting at offset 52
     *
     *  Program Headers:
     *    Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
     *    LOAD           0x001000 0xc0100000 0xc0100000 0x2d4aa 0x2d4aa R E 0x1000
     *    LOAD           0x02f000 0xc012e000 0xc012e000 0x933f9 0x966a4 RW  0x1000
     *
     *   Section to Segment mapping:
     *    Segment Sections...
     *     00     .text .rodata .stab .stabstr
     *     01     .data .bss
    */
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


#include "asm.h"

# Start the CPU: switch to 32-bit protected mode, jump into C.
# The BIOS loads this code from the first sector of the hard disk into
# memory at physical address 0x7c00 and starts executing in real mode
# with %cs=0 %ip=7c00.

.set PROT_MODE_CSEG,        0x8             # 00001 0 00    kernel code segment selector
.set PROT_MODE_DSEG,        0x10            # 00010 0 00    kernel data segment selector
.set CR0_PE_ON,             0x1             # protected mode enable flag
.set SMAP,                  0x534d4150

# start address should be 0:7c00, in real mode, the beginning address of the running bootloader
.globl start
start:
.code16                                             # Assemble for 16-bit mode
    cli                                             # Disable interrupts
    cld                                             # String operations increment

    # Set up the important data segment registers (DS, ES, SS).
    xorw %ax, %ax                                   # Segment number zero
    movw %ax, %ds                                   # -> Data Segment
    movw %ax, %es                                   # -> Extra Segment
    movw %ax, %ss                                   # -> Stack Segment

    # Enable A20:
    #  For backwards compatibility with the earliest PCs, physical
    #  address line 20 is tied low, so that addresses higher than
    #  1MB wrap around to zero by default. This code undoes this.
seta20.1:
    inb $0x64, %al                                  # Wait for not busy
    testb $0x2, %al
    jnz seta20.1

    movb $0xd1, %al                                 # 0xd1 -> port 0x64
    outb %al, $0x64

seta20.2:
    inb $0x64, %al                                  # Wait for not busy
    testb $0x2, %al
    jnz seta20.2

    movb $0xdf, %al                                 # 0xdf -> port 0x60
    outb %al, $0x60

# 探测一个PC机内存的最好方法是通过调用 INT 0x15, eax = 0xe820 来实现
# 当第一次调用这个函数时，将ES：EDI指向列表对应的目的缓冲区，清空EBX，设置EDX
# 的值为0x534d4150,设置EAX的值为0xe820,设置ecx为24，之后就调用中断INT 0x15。
# 如果此中断运行成功，那么eax的值将会被设置为0x534d4150，进位标志被清空，EBX被
# 设置为非0值，这个值将会被保存，使用于下一个函数调用结果是保存在ES：EDI位
# 置的字节长度.为了给函数的后续调用，EDI的值将会以列表项长度增加.同时，重置eax的
# 值为0xe820,ecx为24（ecx的值实际上就是sizeof(struct entryf)）,ebx也许会
# 被设置为0。如果ebx的值被设置为0，那么将会结束函数的运行，否则将会继续进行探测。
# 探测结果存放在 0x8000 处
probe_memory:
    movl $0, 0x8000
    xorl %ebx, %ebx
    movw $0x8004, %di
start_probe:
    movl $0xE820, %eax
    movl $20, %ecx
    movl $SMAP, %edx
    int $0x15
    jnc cont
    movw $12345, 0x8000
    jmp finish_probe
cont:
    addw $20, %di
    incl 0x8000
    cmpl $0, %ebx
    jnz start_probe
finish_probe:

    # Switch from real to protected mode, using a bootstrap GDT
    # and segment translation that makes virtual addresses
    # identical to physical addresses, so that the
    # effective memory map does not change during the switch.
    # 内核基于 32 bit 编译，地址都是虚拟地址，运行内核前必须先打开A20，CR0，进入保护模式
    lgdt gdtdesc # lgdt 0x0000:0x7DC4
    movl %cr0, %eax
    orl $CR0_PE_ON, %eax
    movl %eax, %cr0

    # Jump to next instruction, but in 32-bit code segment.
    # Switches processor into 32-bit mode.
    # 长跳转才能修改 cs 寄存器，这步执行之后，cs 寄存器就指向内核代码段 0x8
    # 指令 jmp 可以实现跨段执行代码，但是它并不能提权（无法改变当权特权级），
    # 也就是说，即便你跨到了 DPL = 0 的段，你的 CPL 也不会发生任何改变。 除非你原来就是 0.
    # 使用 jmp 跨到 0 环代码段，也是有要求的，除非这个0环代码段描述符同意（这就是所谓的一致代码段）。
    ljmp $PROT_MODE_CSEG, $protcseg # jmpf 0x0008:0x7C6D

.code32                                             # Assemble for 32-bit mode
protcseg:
    # Set up the protected-mode data segment registers
    movw $PROT_MODE_DSEG, %ax                       # Our data segment selector
    movw %ax, %ds                                   # -> DS: Data Segment
    movw %ax, %es                                   # -> ES: Extra Segment
    movw %ax, %fs                                   # -> FS
    movw %ax, %gs                                   # -> GS
    movw %ax, %ss                                   # -> SS: Stack Segment

    # Set up the stack pointer and call into C. The stack region is from 0--start(0x7C00)
    # 由于start位置（0x7C00）前的地址空间空闲，可以拿来用作临时栈，栈是向下生长的，不会破坏到start后面的代码
    # 此时只是临时栈，也不能一直往里压数据到太低端的地址，0x0~0x1000的空间里还有bios设置的中断数据 
    # 如果不设置栈，当前的栈还是 bios 设置的栈
    movl $0x0, %ebp     
    movl $start, %esp   # mov sp, 0x7C00
    call bootmain   # call .+116 (0x00007CFE)

    # If bootmain returns (it shouldn't), loop.
spin:
    jmp spin

.data
# Bootstrap GDT，临时 GDT，内核加载之后重新设置 GDT
# 这里内核代码段和内核数据段都没有设置 STA_C，所以是非一致性代码段，不共享，只有 dpl = cpl 才能访问
.p2align 2                                          # force 4 byte alignment
gdt:
    SEG_NULLASM                                     # null seg
    SEG_ASM(STA_X | STA_R , 0x0, 0xffffffff)        # code seg for bootloader and kernel
    SEG_ASM(STA_W , 0x0, 0xffffffff)                # data seg for bootloader and kernel

gdtdesc:
    .word 0x17                                      # sizeof(gdt) - 1
    .long gdt                                       # address gdt 0x7DAC

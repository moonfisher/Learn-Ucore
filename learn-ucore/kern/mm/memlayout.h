#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

/* This file contains the definitions for memory management in our OS. */

/* global segment number */
#define SEG_KTEXT           1
#define SEG_KDATA           2
#define SEG_UTEXT           3
#define SEG_UDATA           4
#define SEG_CALL_GATE       5
#define SEG_TASK_GATE       6
#define SEG_KCPU            7   // kernel per-cpu data
#define SEG_TSS             8

/* global descrptor numbers */
#define GD_KTEXT        ((SEG_KTEXT) << 3)      // kernel text  0x8     00001 0 00
#define GD_KDATA        ((SEG_KDATA) << 3)      // kernel data  0x10    00010 0 00
#define GD_UTEXT        ((SEG_UTEXT) << 3)      // user text    0x18    00011 0 00
#define GD_UDATA        ((SEG_UDATA) << 3)      // user data    0x20    00100 0 00
#define GD_CALL_GATE    ((SEG_CALL_GATE) << 3)  // call segment 0x28    00101 0 00
#define GD_TASK_GATE    ((SEG_TASK_GATE) << 3)  // call segment 0x30    00110 0 00
#define GD_KCPU         ((SEG_KCPU) << 3)       // task segment 0x38    00111 0 00
#define GD_TSS          ((SEG_TSS) << 3)        // task segment 0x40    01000 0 00

/*
 https://blog.csdn.net/qq_37414405/article/details/84535145
 CPL、DPL 和 RPL

 CPL：当前任务特权（Current Privilege Level） 是当前执行的程序或任务的特权级。
 它被存储在 CS 和 SS 的第 0 位和第 1 位上。
 通常情况下，CPL 代表代码所在的段的特权级。当程序转移到不同特权级的代码段时，处理器将改变 CPL。
 只有 0 和 3 两个值，分别表示用户态和内核态。

 DPL：描述符特权（Descriptor Privilege Level） 表示段或门的特权级。
 存储在描述符中的权限位，用于描述代码的所属的特权等级，也就是代码本身真正的特权级。
 一个程序可以使用多个段 (Data，Code，Stack) 也可以只用一个 code 段等。正常的情况下，当程序的
 环境建立好后，段描述符都不需要改变, 当然 DPL 也不需要改变，因此每个段的 DPL 值是固定。
 当前代码段试图访问一个段或者门，DPL 将会和 CPL 以及段或者门选择子的 RPL 相比较，根据段或者门类
 型的不同，DPL 将会区别对待。
 
 RPL：请求特权级 RPL(Request Privilege Level) 是通过段选择子的第 0 和第 1 位表现出来的。
 RPL 保存在选择子的最低两位。 RPL 说明的是进程对段访问的请求权限，意思是当前进程想要的请求权限。
 RPL 的值由程序员自己来自由的设置，并不一定 RPL >= CPL，但是当 RPL < CPL 时，实际起作用的就是
 CPL 了，因为访问时的特权检查是判断：EPL = max(RPL, CPL) <= DPL 是否成立，所以 RPL 可以看成
 是每次访问时的附加限制，RPL = 0 时附加限制最小，RPL = 3 时附加限制最大。所以你不要想通过来随便
 设置一个 RPL 来访问一个比 CPL 更内层的段。
 
 因为你不可能得到比自己更高的权限，你申请的权限一定要比你实际权限低才能通过 CPU 的审查，才能对你放行。
 所以实际上 RPL 的作用是程序员可以把自己的程序降级运行, 有些时候为了更好的安全性, 程序可以在适当的
 时机把自身降低权限（RPL 设成更大的值）。
*/
#define DPL_KERNEL  (0)
#define DPL_USER    (3) // DPL_USER 设置为 1 或者 2 也可以

#define KERNEL_CS   ((GD_KTEXT) | DPL_KERNEL)   // 0x8      00001 0 00
#define KERNEL_DS   ((GD_KDATA) | DPL_KERNEL)   // 0x10     00010 0 00

#define USER_CS     ((GD_UTEXT) | DPL_USER)     // 0x1B     00011 0 11
#define USER_DS     ((GD_UDATA) | DPL_USER)     // 0x23     00100 0 11

/* *
 * Virtual memory map base on 512M physical memory:             Permissions
 *                                                              kernel/user
 *
 *  4G ---------------------> +---------------------------------+ 0xFFFFFFFF
 *                            |                                 |
 *                            |         Empty Memory (*)        |
 *                            |                                 |
 *  VPT end ----------------> +---------------------------------+ 0xFB000000
 *                            |   Cur. Page Table (Kern, RW)    | RW/-- PTSIZE
 *                            |                                 |
 *  vpd end ----------------> |---------------------------------| 0xFAFEBFFF
 *                            |     page directory 1k * 4 = 4k  |
 *  vpd start --------------> |---------------------------------| 0xFAFEB000
 *                            |                                 |
 *                            |---------------------------------| 0xFAFDFFFF
 *                            |  page table 0 ~ 223 (224 * 4k)  |
 *                            |---------------------------------| 0xFAF00000
 *                            |                                 |
 *  VPT start --------------> +---------------------------------+ 0xFAC00000
 *                            |        Invalid Memory (*)       |
 *  MMIOLIM ----------------> +---------------------------------+ 0xF9000000
 *                            |      Memory-mapped I/O 4M       |
 *  MMIOBASE ---------------> +---------------------------------+ 0xF8000000
 *  KERNTOP ----------------> +---------------------------------+ 0xF7FFFFFF -- 896M
 *                            |                                 |
 *                            |                                 |
 *                            |     Remapped Physical Memory    | RW/-- KMEMSIZE
 *                            |                                 |
 *                            |                                 |
 *  physical memory end ----> |---------------------------------| 0xE0000000 512M
 *                            |                                 |
 *  free memory end --------> |---------------------------------| 0xDFFE0000
 *                            |                                 |
 *                            |  free memory = 0x0001F924 * 4k  |
 *                            |                                 |
 *  user_main --------------->|---------------------------------| 0xC06BC4A0
 *                            |                                 |
 *  initproc ---------------->|---------------------------------| 0xC06BC0E8
 *                            |                                 |
 *  idleproc ---------------->|---------------------------------| 0xC06BC008
 *                            |                                 |
 *  free memory start ------> |---------------------------------| 0xC06BC000 
 *                            |                                 |
 *  pages end --------------> |---------------------------------| 0xC06BBB80
 *                            |                                 |
 *                            | sizeof(Page) * npage(0x0001FFE0)|
 *                            |                                 |
 *  pages start ------------> |---------------------------------| 0xC023C000
 *                            |                                 |
 *  end --------------------> |---------------------------------| 0xC0393E70
 *                            |             ......              |
 *                            |---------------------------------|
 *                            |       CPU1's Kernel Stack       |
 *                            |---------------------------------|
 *                            |       CPU0's Kernel Stack       |
 *  percpu_kstacks ---------> |---------------------------------| 0xC0381000
 *                            |                                 |
 *  cpus -------------------> |---------------------------------| 0xC0380020
 *                            |                                 |
 *  proc_list --------------> |---------------------------------| 0xC023B37C
 *                            |                                 |
 *  pra_list_head ----------> |---------------------------------| 0xC023B284
 *                            |                                 |
 *  ticks ------------------> |---------------------------------| 0xC023B130
 *                            |                                 |
 *  wait_queue -------------> |---------------------------------| 0xC023B128
 *                            |                                 |
 *  stdin_buffer -----------> |---------------------------------| 0xC023A120
 *                            |                                 |
 *  disk0_buffer -----------> |---------------------------------| 0xC023A0FC
 *                            |                                 |
 *  vdev_list --------------> |---------------------------------| 0xC023A0C8
 *                            |                                 |
 *  bootfs_node ------------> |---------------------------------| 0xC023A0C4
 *                            |                                 |
 *  timer_list -------------> |---------------------------------| 0xC023A0B4
 *                            |                                 |
 *  hash_list --------------> |---------------------------------| 0xC0238060
 *                            |                                 |
 *  current ----------------> |---------------------------------| 0xC0238048
 *                            |                                 |
 *  ts ---------------------> |---------------------------------| 0xC0237FC0
 *                            |                                 |
 *  idt --------------------> |---------------------------------| 0xC0237780
 *                            |                                 |
 *  ide_devices ------------> |---------------------------------| 0xC0237680
 *                            |                                 |
 *  cons -------------------> |---------------------------------| 0xC0237460
 *                            |                                 |
 *  edata end --------------> |---------------------------------| 0xC0237000
 *                            |       page table 0 ~ 224k       |
 *  __boot_pte -------------> |---------------------------------| 0xC0157000
 *                            |  map (0xC0000000 ~ 0xF8000000)  |
 *  __boot_pgdir[0x300 * 4]-> |---------------------------------| 0xC0156C00
 *                            |   page directory 1k * 4 = 4k    |
 *  __boot_pgdir -----------> |---------------------------------| 0xC0156000
 *                            |                                 |
 *  gdt_pd -----------------> |---------------------------------| 0xC0155A50
 *                            |                                 |
 *  gdt --------------------> |---------------------------------| 0xC0155A20
 *                            |                                 |
 *  __vectors --------------> |---------------------------------| 0xC01555E0
 *                            |                                 |
 *  idt_pd -----------------> |---------------------------------| 0xC0155560
 *                            |                                 |
 *  bootstacktop -----------> |---------------------------------| 0xC0155000
 *                            |          Kernel stack 8k        |
 *  bootstack --------------> |---------------------------------| 0xC0153000
 *                            |                                 |
 *  etext end --------------> |---------------------------------| 0xC011458F
 *                            |                                 |
 *  vector255 --------------> |---------------------------------| 0xC0103768
 *                            |         ISR's entry addrs       |
 *  vector0 ----------------> |---------------------------------| 0xC0102D04
 *                            |                                 |
 *  kern_init --------------> |---------------------------------| 0xC0100036
 *                            |                                 |
 *  kern_entry -------------> |---------------------------------| 0xC0100000
 *                            |                                 |
 *  CGA_BUF ----------------> |---------------------------------| 0xC00B8000
 *                            |                                 |
 *  KERNBASE ---------------> +---------------------------------+ 0xC0000000
 *                            |        Invalid Memory (*)       |
 *  USERTOP ----------------> +---------------------------------+ 0xB0000000
 *                            |      User stack USTACKSIZE      |
 *                            +---------------------------------+ 0xAFF00000
 *                            |         ~~~~~~~~~~~~~~~~        |
 *                            |              MMAP               |
 *                            +---------------------------------+
 *                            |              Heap               |
 *                            |         ~~~~~~~~~~~~~~~~        |
 *                            +---------------------------------+
 *                            |           User Program          |
 *  UTEXT ------------------> +---------------------------------+ 0x00800000
 *                            |        Invalid Memory (*)       | --/--
 *                            |  - - - - - - - - - - - - - - -  |
 *                            |    User STAB Data (optional)    |
 *  USERBASE, USTAB --------> +---------------------------------+ 0x00200000
 *                            |        Invalid Memory (*)       | --/--
 *  0 ----------------------> +---------------------------------+ 0x00000000
 *
 * (*) Note: The kernel ensures that "Invalid Memory" is *never* mapped.
 *  "Empty Memory" is normally unmapped, but user programs may map pages
 *  there if desired.
 *
 * */

/*
 cat /proc/iomem
 00000000-00000fff : reserved
 00001000-0009fbff : System RAM
 0009fc00-0009ffff : reserved
 000a0000-000bffff : PCI Bus 0000:00
 000c0000-000c8bff : Video ROM
 000c9000-000c99ff : Adapter ROM
 000ca000-000cc3ff : Adapter ROM
 000f0000-000fffff : reserved
 000f0000-000fffff : System ROM
 00100000-3fffdfff : System RAM
 01000000-0169c350 : Kernel code
 0169c351-01af38bf : Kernel data
 01ca2000-01fa1fff : Kernel bss
 3fffe000-3fffffff : reserved
 40000000-febfffff : PCI Bus 0000:00
 fc000000-fdffffff : 0000:00:02.0
 fc000000-fdffffff : cirrusdrmfb_vram
 febd0000-febdffff : 0000:00:02.0
 febe0000-febeffff : 0000:00:03.0
 febf0000-febf0fff : 0000:00:02.0
 febf0000-febf0fff : cirrusdrmfb_mmio
 febf1000-febf1fff : 0000:00:03.0
 febf2000-febf2fff : 0000:00:04.0
 fec00000-fec003ff : IOAPIC 0
 fee00000-fee00fff : Local APIC
 feffc000-feffffff : reserved
 fffc0000-ffffffff : reserved
*/

/*
 I/O 端口地址表
 
 PC 只用了 10 位地址线 (A0-A9) 进行译码，其寻址的范围为 0H-3FFH，共有 1024 个 I/O 地址。
 这 1024 个地址中前半段 (A9=0，范围为0H-1FFH) 是属于主机板 I/O 译码，
 后半段 (A9=1，范围为200H-3FFH) 则是用来扩展插槽上的 I/O 译码用。
 
 I/O 端口功能表  cat /proc/ioports
 ———————————————————————————
 I/O 地址　功能、用途
 ———————————————————————————
 0　　　　      DMA通道0，内存地址寄存器（DMA控制器1(8237)）
 1　　　　      DMA通道0, 传输计数寄存器
 2　　　　      DMA通道1，内存地址寄存器
 3　　　　      DMA通道1, 传输计数寄存器
 4　　　　      DMA通道2，内存地址寄存器
 5　　　　      DMA通道2, 传输计数寄存器
 6　　　　      DMA通道3，内存地址寄存器
 7　　　　      DMA通道3, 传输计数寄存器
 8　　　　      DMA通道0-3的状态寄存器
 0AH           DMA通道0-3的屏蔽寄存器
 0BH　　　      DMA通道0-3的方式寄存器
 0CH　　　      DMA清除字节指针
 0DH　　　      DMA主清除字节
 0EH　　　      DMA通道0-3的清屏蔽寄存器
 0FH　　　      DMA通道0-3的写屏蔽寄存器
 19H　　　      DMA起始寄存器
 20H-3FH　     可编程中断控制器1(8259)使用
 40H　　　      可编程中断计时器(8253)使用，读/写计数器0
 41H　　　      可编程中断计时器寄存器
 42H　　　      可编程中断计时器杂项寄存器
 43H　　　      可编程中断计时器,控制字寄存器
 44H　　　      可编程中断计时器,杂项寄存器（AT）
 47H　　　      可编程中断计时器,计数器0的控制字寄存器
 48H-5FH       可编程中断计时器使用
 60H-61H　     键盘输入数据缓冲区
 61H　　　      AT:8042键盘控制寄存器/XT:8255输出寄存器
 62H　　　      8255输入寄存器
 63H　　　      8255命令方式寄存器
 64H　　　      8042键盘输入缓冲区/8042状态
 65H-6FH　     8255/8042专用
 70H　　　      CMOS RAM地址寄存器
 71H　　　      CMOS RAM数据寄存器
 80H　　　      生产测试端口
 81H　　　      DMA通道2,页表地址寄存器
 82H　　　      DMA通道3,页表地址寄存器
 83H　　　      DMA通道1,页表地址寄存器
 87H　　　      DMA通道0,页表地址寄存器
 89H　　　      DMA通道6,页表地址寄存器
 8AH　　　      DMA通道7,页表地址寄存器
 8BH　　　      DMA通道5,页表地址寄存器
 8FH　　　      DMA通道4,页表地址寄存器
 93H-9FH　     DMA控制器专用
 0A0H　　　     NM1屏蔽寄存器/可编程中断控制器2
 0A1H　　　     可编程中断控制器2屏蔽
 0C0H　　　     DMA通道0，内存地址寄存器（DMA控制器2(8237)）
 0C2H　　　     DMA通道0, 传输计数寄存器
 0C4H　　　     DMA通道1，内存地址寄存器
 0C6H　　　     DMA通道1, 传输计数寄存器
 0C8H　　　     DMA通道2，内存地址寄存器
 0CAH　　　     DMA通道2, 传输计数寄存器
 0CCH　　　     DMA通道3，内存地址寄存器
 0CEH　　　     DMA通道3, 传输计数寄存器
 0D0H　　　     DMA状态寄存器
 0D2H　　　     DMA写请求寄存器
 0D4H　　　     DMA屏蔽寄存器
 0D6H　　　     DMA方式寄存器
 0D8H　　　     DMA清除字节指针
 0DAH　　　     DMA主清
 0DCH　　　     DMA清屏蔽寄存器
 0DEH　　　     DMA写屏蔽寄存器
 0DFH-0EFH　   保留
 0F0H-0FFH　   协处理器使用
 100H-16FH     保留
 170H　　      1号硬盘数据寄存器
 171H　　      1号硬盘错误寄存器
 172H　　      1号硬盘数据扇区计数
 173H　　      1号硬盘扇区数
 174H　　      1号硬盘柱面（低字节）
 175H　　      1号硬盘柱面（高字节）
 176H　　      1号硬盘驱动器/磁头寄存器
 177H　　      1号硬盘状态寄存器
 1F0H　　      0号硬盘数据寄存器
 1F1H　　      0号硬盘错误寄存器
 1F2H　　      0号硬盘数据扇区计数
 1F3H　　      0号硬盘扇区数
 1F4H　　      0号硬盘柱面（低字节）
 1F5H　　      0号硬盘柱面（高字节）
 1F6H　　      0号硬盘驱动器/磁头寄存器
 1F7H　　      0号硬盘状态寄存器
 1F9H-1FFH    保留
 200H-20FH    游戏控制端口
 210H-21FH    扩展单元
 278H　　      3号并行口，数据端口
 279H　　      3号并行口，状态端口
 27AH　　      3号并行口，控制端口
 2B0H-2DFH    保留
 2E0H　　      EGA/VGA使用
 2E1H　　      GPIP(0号适配器)
 2E2H　　      数据获取(0号适配器)
 2E3H　　      数据获取(1号适配器)
 2E4H-2F7H    保留
 2F8H　　      2号串行口，发送/保持寄存器(RS232接口卡2)
 2F9H　　      2号串行口，中断有效寄存器
 2FAH　　      2号串行口，中断ID寄存器
 2FBH　　      2号串行口，线控制寄存器
 2FCH　　      2号串行口，调制解调控制寄存器
 2FDH　　      2号串行口，线状态寄存器
 2FEH　　      2号串行口，调制解调状态寄存器
 2FFH　　      保留
 300H-31FH    原形卡
 320H　　      硬盘适配器寄存器
 322H　　      硬盘适配器控制/状态寄存器
 324H　　      硬盘适配器提示/中断状态寄存器
 325H-347H    保留
 348H-357H　  DCA3278
 366H-36FH　  PC网络
 372H　　　    软盘适配器数据输出/状态寄存器
 375H-376H　  软盘适配器数据寄存器
 377H　　　    软盘适配器数据输入寄存器
 378H　　　    2号并行口，数据端口
 379H　　　    2号并行口，状态端口
 37AH　　　    2号并行口，控制端口
 380H-38FH　  SDLC及BSC通讯
 390H-393H　  Cluster适配器0
 3A0H-3AFH　  BSC通讯
 3B0H-3BFH　  MDA视频寄存器
 3BCH　　　    1号并行口，数据端口
 3BDH　　　    1号并行口，状态端口
 3BEH　　　    1号并行口，控制端口
 3C0H-3CFH　  EGA/VGA视频寄存器
 3D0H-3D7H　  CGA视频寄存器
 3F0H-3F7H　  软盘控制器寄存器
 3F8H　　　    1号串行口，发送/保持寄存器(RS232接口卡1)
 3F9H　　　    1号串行口，中断有效寄存器
 3FAH　　　    1号串行口，中断ID寄存器
 3FBH　　　    1号串行口，线控制寄存器
 3FCH　　　    1号串行口，调制解调控制寄存器
 3FDH　　　    1号串行口，线状态寄存器
 3FEH　　　    1号串行口，调制解调状态寄存器
 3FFH　　　    保留
 
 cat /proc/ioports
 
 0000-0cf7 : PCI Bus 0000:00
 0000-001f : dma1
 0020-003f : pic1
 0040-005f : timer
 0060-006f : keyboard
 0070-007f : rtc
 0080-008f : dma page reg
 00a0-00bf : pic2
 00c0-00df : dma2
 00f0-00ff : fpu
 0170-0177 : ide1
 01f0-01f7 : ide0
 02f8-02ff : serial(auto)
 0376-0376 : ide1
 03c0-03df : vga+
 03f6-03f6 : ide0
 03f8-03ff : serial(auto)
 0500-051f : PCI device 8086:24d3 (Intel Corp.)
 0cf8-0cff : PCI conf1
 0d00-ffff : PCI Bus 0000:00
 afe0-afe3 : ACPI GPE0_BLK
 b000-b03f : 0000:00:01.3
 b000-b003 : ACPI PM1a_EVT_BLK
 b004-b005 : ACPI PM1a_CNT_BLK
 b008-b00b : ACPI PM_TMR
 b010-b015 : ACPI CPU throttle
 b100-b10f : 0000:00:01.3
 b100-b107 : piix4_smbus
 c000-c03f : 0000:00:04.0
 c000-c03f : virtio-pci-legacy
 c040-c05f : 0000:00:01.2
 c040-c05f : uhci_hcd
 c060-c07f : 0000:00:03.0
 c060-c07f : virtio-pci-legacy
 c080-c09f : 0000:00:05.0
 c080-c09f : virtio-pci-legacy
 c0a0-c0af : 0000:00:01.1
 c0a0-c0af : ata_piix
 da00-daff : VIA Technologies, Inc. VT6102 [Rhine-II]
 da00-daff : via-rhine
 e000-e01f : PCI device 8086:24d4 (Intel Corp.)
 e000-e01f : usb-uhci
 e100-e11f : PCI device 8086:24d7 (Intel Corp.)
 e100-e11f : usb-uhci
 e200-e21f : PCI device 8086:24de (Intel Corp.)
 e200-e21f : usb-uhci
 e300-e31f : PCI device 8086:24d2 (Intel Corp.)
 e300-e31f : usb-uhci
 f000-f00f : PCI device 8086:24db (Intel Corp.)
 f000-f007 : ide0
 f008-f00f : ide1
*/

/*
 cat /proc/interrupts
   0:         78   IO-APIC-edge      timer
   1:         10   IO-APIC-edge      i8042
   4:        786   IO-APIC-edge      serial
   6:          3   IO-APIC-edge      floppy
   8:          0   IO-APIC-edge      rtc0
   9:          0   IO-APIC-fasteoi   acpi
  10:          0   IO-APIC-fasteoi   virtio2
  11:          0   IO-APIC-fasteoi   uhci_hcd:usb1
  12:         15   IO-APIC-edge      i8042
  14:          0   IO-APIC-edge      ata_piix
  15:          0   IO-APIC-edge      ata_piix
  24:          0   PCI-MSI-edge      virtio0-config
  25:  226824135   PCI-MSI-edge      virtio0-input.0
  26:       3892   PCI-MSI-edge      virtio0-output.0
  27:          0   PCI-MSI-edge      virtio1-config
  28:  171752297   PCI-MSI-edge      virtio1-req.0
 NMI:          0   Non-maskable interrupts
 LOC:  858246000   Local timer interrupts
 SPU:          0   Spurious interrupts
 PMI:          0   Performance monitoring interrupts
 IWI:  188120746   IRQ work interrupts
 RTR:          0   APIC ICR read retries
 RES:          0   Rescheduling interrupts
 CAL:          0   Function call interrupts
 TLB:          0   TLB shootdowns
 TRM:          0   Thermal event interrupts
 THR:          0   Threshold APIC interrupts
 DFR:          0   Deferred Error APIC interrupts
 MCE:          0   Machine check exceptions
 MCP:     149737   Machine check polls
 ERR:          0
 MIS:          0
 PIN:          0   Posted-interrupt notification event
 PIW:          0   Posted-interrupt wakeup event
*/

/*
 Linux 操作系统和驱动程序运行在内核空间，应用程序运行在用户空间，两者不能简单地使用指针传递数据，
 因为Linux使用的虚拟内存机制，用户空间的数据可能被换出，当内核空间使用用户空间指针时，
 对应的数据可能不在内存中。用户空间的内存映射采用段页式，而内核空间有自己的规则
 用户进程没有高端内存概念。只有在内核空间才存在高端内存。
 用户进程最多只可以访问3G物理内存，而内核进程可以访问所有物理内存。
*/
/* All physical memory mapped at this address */
#define KERNBASE            0xC0000000
/*
 为什么上限是896M，Linux内核高端内存的由来 Linux HighMemory（高端内存）
 当内核模块代码或线程访问内存时，代码中的内存地址都为逻辑地址，而对应到真正的物理内存地址，
 需要地址一对一的映射，逻辑地址与物理地址对应的关系为：
 物理地址 = 逻辑地址 – 0xC0000000
 这是内核地址空间的地址转换关系，注意内核的虚拟地址在“高端”，但是内核映射的物理内存地址在低端。
 https://blog.csdn.net/qq_26222859/article/details/80901104
*/
#define KMEMSIZE            0x38000000  // the maximum amount of physical memory
#define KERNTOP             (KERNBASE + KMEMSIZE)

/* *
 * Virtual page table. Entry PDX[VPT] in the PD (Page Directory) contains
 * a pointer to the page directory itself, thereby turning the PD into a page
 * table, which maps all the PTEs (Page Table Entry) containing the page mappings
 * for the entire virtual address space into that 4 Meg region starting at VPT.
 * */
#define VPT                 0xFAC00000

// At IOPHYSMEM (640K) there is a 384K hole for I/O.  From the kernel,
// IOPHYSMEM can be addressed at KERNBASE + IOPHYSMEM.  The hole ends
// at physical address EXTPHYSMEM.
#define IOPHYSMEM           0x0A0000
#define EXTPHYSMEM          0x100000

// e1000's physical base address = febc0000  and size = 0x20000

#define MMIOBASE            KERNTOP             // 0xF8000000
#define MMIOLIM             MMIOBASE + PTSIZE   // 0xF9000000

// Physical address of startup code for non-boot CPUs (APs)
#define MPENTRY_PADDR       0x7000

// Kernel stack.
#define KSTACKTOP           KERNBASE
#define KSTACKPAGE          2                           // # of pages in kernel stack
#define KSTACKSIZE          (KSTACKPAGE * PGSIZE)       // size of a kernel stack
#define KSTACKGAP           KSTACKSIZE                  // size of a kernel stack guard

// User stack.
#define USERTOP             0xB0000000
#define USTACKTOP           USERTOP
#define USTACKPAGE          256                         // # of pages in user stack
#define USTACKSIZE          (USTACKPAGE * PGSIZE)       // sizeof user stack

#define USERBASE            0x00200000
#define UTEXT               0x00800000                  // where user programs generally begin
#define USTAB               USERBASE                    // the location of the user STABS data structure

#define USER_ACCESS(start, end)                     \
(USERBASE <= (start) && (start) < (end) && (end) <= USERTOP)

#define KERN_ACCESS(start, end)                     \
(KERNBASE <= (start) && (start) < (end) && (end) <= KERNTOP)

#ifndef __ASSEMBLER__

#include "defs.h"
#include "atomic.h"
#include "list.h"

typedef uintptr_t pte_t;                                                          
typedef uintptr_t pde_t;
typedef pte_t swap_entry_t; //the pte can also be a swap entry

// some constants for bios interrupt 15h AX = 0xE820
#define E820MAX             20      // number of entries in E820MAP
#define E820_ARM            1       // address range memory
#define E820_ARR            2       // address range reserved

struct e820map
{
    int nr_map;
    struct
    {
        uint64_t addr;
        uint64_t size;
        uint32_t type;
    } __attribute__((packed)) map[E820MAX];
};

/* *
 * struct Page - Page descriptor structures. Each Page describes one
 * physical page. In kern/mm/pmm.h, you can find lots of useful functions
 * that convert Page to other data types, such as phyical address.
 * */
/*
 由于一个物理页可能被映射到不同的虚拟地址上去（譬如一块内存在不同进程间共享）
 当这个页需要在一个地址上解除映射时，操作系统不能直接把这个页回收，
 而是要先看看它还有没有映射到别的虚拟地址上。
 这是通过查找管理该物理页的 Page 数据结构的成员变量 ref 来实现的
 */
struct Page
{
    int ref;                        // 物理页被引用次数
    uint32_t flags;                 // 当前页的状态
    unsigned int property;          // used in buddy system, stores the order (the X in 2^X) of the continuous memory block 用来记录某连续内存空闲块的大小（即地址连续的空闲页的个数）
    int zone_num;                   // used in buddy system, the No. of zone which the page belongs to
    list_entry_t page_link;         // 连接到全局空闲内存链表
    // 如果当前页面可以被交换到 swap 区，则链接到 swap manager
    list_entry_t pra_page_link;     // used for pra (page replace algorithm)
    // 可交换到 swap 区的页面地址
    uintptr_t pra_vaddr;            // used for pra (page replace algorithm)
};

/* Flags describing the status of a page frame */
#define PG_reserved                 0       // the page descriptor is reserved for kernel or unusable
#define PG_property                 1       // the member 'property' is valid
#define PG_slab                     2       // page frame is included in a slab
#define PG_dirty                    3       // the page has been modified
#define PG_swap                     4       // the page is in the active or inactive page list (and swap hash table)

#define SetPageReserved(page)       set_bit(PG_reserved, &((page)->flags))
#define ClearPageReserved(page)     clear_bit(PG_reserved, &((page)->flags))
#define PageReserved(page)          test_bit(PG_reserved, &((page)->flags))
#define SetPageProperty(page)       set_bit(PG_property, &((page)->flags))
#define ClearPageProperty(page)     clear_bit(PG_property, &((page)->flags))
#define PageProperty(page)          test_bit(PG_property, &((page)->flags))
#define SetPageSlab(page)           set_bit(PG_slab, &((page)->flags))
#define ClearPageSlab(page)         clear_bit(PG_slab, &((page)->flags))
#define PageSlab(page)              test_bit(PG_slab, &((page)->flags))
#define SetPageDirty(page)          set_bit(PG_dirty, &((page)->flags))
#define PageSwap(page)              test_bit(PG_swap, &((page)->flags))

// convert list entry to page
#define le2page(le, member)                 \
    to_struct((le), struct Page, member)

/* free_area_t - maintains a doubly linked list to record free (unused) pages */
typedef struct
{
    list_entry_t free_list;         // the list header
    unsigned int nr_free;           // # of free pages in this free list
} free_area_t;

/* for slab style kmalloc */
//#define PG_slab                     2       // page frame is included in a slab
//#define SetPageSlab(page)           set_bit(PG_slab, &((page)->flags))
//#define ClearPageSlab(page)         clear_bit(PG_slab, &((page)->flags))
//#define PageSlab(page)              test_bit(PG_slab, &((page)->flags))

#endif /* !__ASSEMBLER__ */

#endif /* !__KERN_MM_MEMLAYOUT_H__ */


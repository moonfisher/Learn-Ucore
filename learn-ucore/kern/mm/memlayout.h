#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

/* This file contains the definitions for memory management in our OS. */

/* global segment number */
#define SEG_KTEXT   1
#define SEG_KDATA   2
#define SEG_UTEXT   3
#define SEG_UDATA   4
#define SEG_TSS     5
#define SEG_CALL    6

/* global descrptor numbers */
#define GD_KTEXT    ((SEG_KTEXT) << 3)      // kernel text              0x8     00001 0 00
#define GD_KDATA    ((SEG_KDATA) << 3)      // kernel data              0x10    00010 0 00
#define GD_UTEXT    ((SEG_UTEXT) << 3)      // user text                0x18    00011 0 00
#define GD_UDATA    ((SEG_UDATA) << 3)      // user data                0x20    00100 0 00
#define GD_TSS      ((SEG_TSS) << 3)        // task segment selector    0x28    00101 0 00
#define GD_CALL     ((SEG_CALL) << 3)       // call segment selector    0x30    00110 0 00

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
 *  vpd start --------------> |---------------------------------| 0xFAFEB000 (__boot_pgdir)
 *                            |                                 |
 *                            |---------------------------------| 0xFAFDFFFF
 *                            |  page table 0 ~ 223 (224 * 4k)  |
 *                            |---------------------------------| 0xFAF00000
 *                            |                                 |
 *  VPT start --------------> +---------------------------------+ 0xFAC00000
 *                            |        Invalid Memory (*)       | --/--
 *  KERNTOP ----------------> +---------------------------------+ 0xF7FFFFFF -- 896M
 *                            |                                 |
 *                            |                                 |
 *                            |     Remapped Physical Memory    | RW/-- KMEMSIZE
 *                            |                                 |
 *                            |                                 |
 *  physical memory end ----> |---------------------------------| 0xE0000000 512M
 *                            |                                 |
 *  free memory end --------> |---------------------------------| 0xDFFE0000
 *                            |  free memory = 0x0001F924 * 4k  |
 *                            |                                 |
 *  user_main --------------->|---------------------------------| 0xC06BC4A0
 *                            |                                 |
 *  initproc ---------------->|---------------------------------| 0xC06BC0E8
 *                            |                                 |
 *  idleproc ---------------->|---------------------------------| 0xC06BC008
 *                            |                                 |
 *  free memory start ------> |---------------------------------| 0xC06BC000 (0x0001F924 * 4k)
 *                            |                                 |
 *  pages end --------------> |---------------------------------| 0xC06BBB80
 *                            |                                 |
 *                            | sizeof(Page) * npage(0x0001FFE0)|
 *                            |                                 |
 *  pages start ------------> |---------------------------------| 0xC023C000
 *                            |                                 |
 *  end --------------------> |---------------------------------| 0xC023B384
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
 *                            |        Invalid Memory (*)       | --/--
 *  USERTOP ----------------> +---------------------------------+ 0xB0000000
 *                            |           User stack            |
 *                            +---------------------------------+
 *                            |                                 |
 *                            :                                 :
 *                            |         ~~~~~~~~~~~~~~~~        |
 *                            :                                 :
 *                            |                                 |
 *                            ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                            |       User Program & Heap       |
 *  UTEXT ------------------> +---------------------------------+ 0x00800000
 *                            |        Invalid Memory (*)       | --/--
 *                            |  - - - - - - - - - - - - - - -  |
 *                            |    User STAB Data (optional)    |
 *  USERBASE, USTAB---------> +---------------------------------+ 0x00200000
 *                            |        Invalid Memory (*)       | --/--
 *  0 ----------------------> +---------------------------------+ 0x00000000
 *
 * (*) Note: The kernel ensures that "Invalid Memory" is *never* mapped.
 *  "Empty Memory" is normally unmapped, but user programs may map pages
 *  there if desired.
 *
 * */

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
#define KMEMSIZE            0x38000000                  // the maximum amount of physical memory
#define KERNTOP             (KERNBASE + KMEMSIZE)

/* *
 * Virtual page table. Entry PDX[VPT] in the PD (Page Directory) contains
 * a pointer to the page directory itself, thereby turning the PD into a page
 * table, which maps all the PTEs (Page Table Entry) containing the page mappings
 * for the entire virtual address space into that 4 Meg region starting at VPT.
 * */
#define VPT                 0xFAC00000

#define KSTACKPAGE          2                           // # of pages in kernel stack
#define KSTACKSIZE          (KSTACKPAGE * PGSIZE)       // sizeof kernel stack

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

#define SetPageReserved(page)       set_bit(PG_reserved, &((page)->flags))
#define ClearPageReserved(page)     clear_bit(PG_reserved, &((page)->flags))
#define PageReserved(page)          test_bit(PG_reserved, &((page)->flags))
#define SetPageProperty(page)       set_bit(PG_property, &((page)->flags))
#define ClearPageProperty(page)     clear_bit(PG_property, &((page)->flags))
#define PageProperty(page)          test_bit(PG_property, &((page)->flags))

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


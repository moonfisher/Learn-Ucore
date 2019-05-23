#ifndef __KERN_MM_MMU_H__
#define __KERN_MM_MMU_H__

/*
 CPU 是如何访问到内存的？-- MMU 最基本原理
 http://www.10tiao.com/html/606/201802/2664605117/1.html
*/

/* Eflags register */
#define FL_CF           0x00000001  // Carry Flag
#define FL_PF           0x00000004  // Parity Flag
#define FL_AF           0x00000010  // Auxiliary carry Flag
#define FL_ZF           0x00000040  // Zero Flag
#define FL_SF           0x00000080  // Sign Flag
#define FL_TF           0x00000100  // Trap Flag
#define FL_IF           0x00000200  // Interrupt Flag
#define FL_DF           0x00000400  // Direction Flag
#define FL_OF           0x00000800  // Overflow Flag
#define FL_IOPL_MASK    0x00003000  // I/O Privilege Level bitmask
#define FL_IOPL_0       0x00000000  //   IOPL == 0
#define FL_IOPL_1       0x00001000  //   IOPL == 1
#define FL_IOPL_2       0x00002000  //   IOPL == 2
#define FL_IOPL_3       0x00003000  //   IOPL == 3
#define FL_NT           0x00004000  // Nested Task
#define FL_RF           0x00010000  // Resume Flag
#define FL_VM           0x00020000  // Virtual 8086 mode
#define FL_AC           0x00040000  // Alignment Check
#define FL_VIF          0x00080000  // Virtual Interrupt Flag
#define FL_VIP          0x00100000  // Virtual Interrupt Pending
#define FL_ID           0x00200000  // ID flag

/* Application segment type bits */
#define STA_X           0x8         // Executable segment
#define STA_E           0x4         // Expand down (non-executable segments)
#define STA_C           0x4         // Conforming code segment (executable only)
#define STA_W           0x2         // Writeable (non-executable segments)
#define STA_R           0x2         // Readable (executable segments)
#define STA_A           0x1         // Accessed

/* System segment type bits */
#define STS_T16A        0x1         // Available 16-bit TSS
#define STS_LDT         0x2         // Local Descriptor Table
#define STS_T16B        0x3         // Busy 16-bit TSS
#define STS_CG16        0x4         // 16-bit Call Gate
#define STS_TG          0x5         // Task Gate / Coum Transmitions
#define STS_IG16        0x6         // 16-bit Interrupt Gate
#define STS_TG16        0x7         // 16-bit Trap Gate
#define STS_T32A        0x9         // Available 32-bit TSS
#define STS_T32B        0xB         // Busy 32-bit TSS
#define STS_CG32        0xC         // 32-bit Call Gate
#define STS_IG32        0xE         // 32-bit Interrupt Gate
#define STS_TG32        0xF         // 32-bit Trap Gate

#ifdef __ASSEMBLER__

#define SEG_NULL                                                \
    .word 0, 0;                                                 \
    .byte 0, 0, 0, 0

#define SEG_ASM(type,base,lim)                                  \
    .word (((lim) >> 12) & 0xffff), ((base) & 0xffff);          \
    .byte (((base) >> 16) & 0xff), (0x90 | (type)),             \
        (0xC0 | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)

#else /* not __ASSEMBLER__ */

#include "defs.h"

/* Gate descriptors for interrupts and traps */
struct gatedesc
{
    unsigned gd_off_15_0 : 16;      // low 16 bits of offset in segment
    unsigned gd_ss : 16;            // segment selector
    unsigned gd_args : 5;           // # args, 0 for interrupt/trap gates
    unsigned gd_rsv1 : 3;           // reserved(should be zero I guess)
    unsigned gd_type : 4;           // type(STS_{TG,IG32,TG32})
    unsigned gd_s : 1;              // must be 0 (system)
    unsigned gd_dpl : 2;            // descriptor(meaning new) privilege level
    unsigned gd_p : 1;              // Present
    unsigned gd_off_31_16 : 16;     // high bits of offset in segment
};

/* segment descriptors */
struct segdesc
{
    unsigned sd_lim_15_0 : 16;      // low bits of segment limit
    unsigned sd_base_15_0 : 16;     // low bits of segment base address
    unsigned sd_base_23_16 : 8;     // middle bits of segment base address
    /*
     sd_type 指明段或者门的类型，确定段的范围权限和增长方向。
     如何解释这个域，取决于该描述符是应用描述符(代码或数据)还是系统描述符，
     这由描述符类型标志(S 标记)所确定。代码段，数据段和系统段对类型域有不同的意义。
     
     对于一致代码段(C 位为 1)，也就是共享的段，一致代码段就是系统用来共享、提供给低特权级
     的程序使用调用的代码
     特权级高的程序不允许访问特权级低的数据，核心态不允许调用用户态的数据.
     特权级低的程序可以访问到特权级高的数据，但是特权级不会改变，用户态还是用户态.
     
     对于普通代码段(C 位为 0)，也就是非一致代码段，为了避免被低特权级程序访问而被系统保护起来的代码。
     只允许同级间访问，DPL 规定了可以访问该段的特权级，如果 DPL 为 1，那么只有运行在 CPL
     为 1 的程序才有权访问它。
     绝对禁止不同级访问，核心态不用用户态，用户态也不使用核心态.
     
     绝大多数代码段都是不一致的（non conforming），这些代码只能在同优先级的代码中调用，除非
     使用调用门（call gate）。
    */
    unsigned sd_type : 4;           // segment type (see STS_ constants)
    /*
     sd_s 确定段描述符是系统描述符(S 标记为 0)或者代码，数据段描述符(S 标记为 1)
     系统段描述符指的就是 LDT 描述符和 TSS 描述符，LDT 描述符和 TSS 描述符也放在全局描述
     符 GDT 中。每个任务都有自己的局部描述符表 LDT 和任务状态段 TSS，LDT 和 TSS 是内存中
     特殊的段，它们有起始地址、大小和属性，也用描述符来描述它们，所有有 LDT 描述符和 TSS 描述符
    */
    unsigned sd_s : 1;              // 0 = system, 1 = application
    // sd_dpl 指明该段的特权级。特权级从 0~3，0 为最高特权级。DPL 用来控制对该段的访问
    unsigned sd_dpl : 2;            // descriptor Privilege Level
    /*
     sd_p 标志指出该段当前是否在内存中(1 表示在内存中，0 表示不在)。当指向该段描述符的
     段选择符装载人段寄存器时，如果这个标志为 0，处理器会产生一个段不存在异常(NP)。 内存
     管理软件可以通过这个标志，来控制在某个特定时间有哪些段是真正的被载入物理内存，这样对于
     管理虚拟内存而言，除了分页机制还提供了另一种控制方法。
    */
    unsigned sd_p : 1;              // present
    unsigned sd_lim_19_16 : 4;      // high bits of segment limit
    unsigned sd_avl : 1;            // unused (available for software use)
    unsigned sd_rsv1 : 1;           // reserved
    /*
     sd_db 根据这个段描述符所指的是一个可执行代码段，一个向下扩展的数据段还是一个堆栈段，
     这个标志完成不同的功能。(对32位的代码和数据段，这个标志总是被置为1，而 16位的代码和
     数据段，这个标志总是被置为0)
    */
    unsigned sd_db : 1;             // 0 = 16-bit segment, 1 = 32-bit segment
    /*
     sd_g 确定段限长扩展的增量。
     当 G 标志为 0，段限长以字节为单位;
     G 标志为 1，段限长以 4KB 为单位。(这个标志不影响段基址的粒度，段基址的粒度永远是字节)
     如果G标志为 1， 那么当检测偏移量是否超越段限长时，不用测试偏移量的低 12 位。
     例如，如果 G 标志为 1， 0 段限长意味着有效偏移量为从 0 到 4095。
    */
    unsigned sd_g : 1;              // granularity: limit scaled by 4K when set
    unsigned sd_base_31_24 : 8;     // high bits of segment base address
};

#define SEG_NULL                                            \
    (struct segdesc) {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

#define SEG(type, base, lim, dpl)                           \
    (struct segdesc) {                                      \
        ((lim) >> 12) & 0xffff, (base) & 0xffff,            \
        ((base) >> 16) & 0xff, type, 1, dpl, 1,             \
        (unsigned)(lim) >> 28, 0, 0, 1, 1,                  \
        (unsigned) (base) >> 24                             \
    }

/* task state segment format (as described by the Pentium architecture book) */
// 这个结构根据 tss 来定义
struct taskstate
{
    uint32_t ts_link;       // old ts selector
    uintptr_t ts_esp0;      // stack pointers and segment selectors
    uint16_t ts_ss0;        // after an increase in privilege level
    uint16_t ts_padding1;
    uintptr_t ts_esp1;
    uint16_t ts_ss1;
    uint16_t ts_padding2;
    uintptr_t ts_esp2;
    uint16_t ts_ss2;
    uint16_t ts_padding3;
    uintptr_t ts_cr3;       // page directory base
    uintptr_t ts_eip;       // saved state from last task switch
    uint32_t ts_eflags;
    uint32_t ts_eax;        // more saved state (registers)
    uint32_t ts_ecx;
    uint32_t ts_edx;
    uint32_t ts_ebx;
    uintptr_t ts_esp;
    uintptr_t ts_ebp;
    uint32_t ts_esi;
    uint32_t ts_edi;
    uint16_t ts_es;         // even more saved state (segment selectors)
    uint16_t ts_padding4;
    uint16_t ts_cs;
    uint16_t ts_padding5;
    uint16_t ts_ss;
    uint16_t ts_padding6;
    uint16_t ts_ds;
    uint16_t ts_padding7;
    uint16_t ts_fs;
    uint16_t ts_padding8;
    uint16_t ts_gs;
    uint16_t ts_padding9;
    uint16_t ts_ldt;
    uint16_t ts_padding10;
    uint16_t ts_t;          // trap on task switch
    uint16_t ts_iomb;       // i/o map base address
} __attribute__((packed));

#endif /* !__ASSEMBLER__ */

// A linear address 'la' has a three-part structure as follows:
//
// +--------10------+-------10-------+---------12----------+
// | Page Directory |   Page Table   | Offset within Page  |
// |      Index     |     Index      |                     |
// +----------------+----------------+---------------------+
//  \--- PDX(la) --/ \--- PTX(la) --/ \---- PGOFF(la) ----/
//  \----------- PPN(la) -----------/
//
// The PDX, PTX, PGOFF, and PPN macros decompose linear addresses as shown.
// To construct a linear address la from PDX(la), PTX(la), and PGOFF(la),
// use PGADDR(PDX(la), PTX(la), PGOFF(la)).

// page directory index
#define PDX(la) ((((uintptr_t)(la)) >> PDXSHIFT) & 0x3FF)

// page table index
#define PTX(la) ((((uintptr_t)(la)) >> PTXSHIFT) & 0x3FF)

// page number field of address
#define PPN(la) (((uintptr_t)(la)) >> PTXSHIFT)

// offset in page
#define PGOFF(la) (((uintptr_t)(la)) & 0xFFF)

// construct linear address from indexes and offset
#define PGADDR(d, t, o) ((uintptr_t)((d) << PDXSHIFT | (t) << PTXSHIFT | (o)))

// address in page table or page directory entry
#define PTE_ADDR(pte)   ((uintptr_t)(pte) & ~0xFFF)
#define PDE_ADDR(pde)   PTE_ADDR(pde)

/* page directory and page table constants */
#define NPDEENTRY       1024                    // page directory entries per page directory
#define NPTEENTRY       1024                    // page table entries per page table

#define PGSIZE          4096                    // bytes mapped by a page
#define PGSHIFT         12                      // log2(PGSIZE)
#define PTSIZE          (PGSIZE * NPTEENTRY)    // bytes mapped by a page directory entry
#define PTSHIFT         22                      // log2(PTSIZE)

#define PTXSHIFT        12                      // offset of PTX in a linear address
#define PDXSHIFT        22                      // offset of PDX in a linear address

/* page table/directory entry flags */
#define PTE_P           0x001                   // Present
#define PTE_W           0x002                   // Writeable
#define PTE_U           0x004                   // User
#define PTE_PWT         0x008                   // Write-Through
#define PTE_PCD         0x010                   // Cache-Disable
#define PTE_A           0x020                   // Accessed
#define PTE_D           0x040                   // Dirty
#define PTE_PS          0x080                   // Page Size
#define PTE_MBZ         0x180                   // Bits must be zero
#define PTE_AVAIL       0xE00                   // Available for software use
                                                // The PTE_AVAIL bits aren't used by the kernel or interpreted by the
                                                // hardware, so user processes are allowed to set them arbitrarily.

#define PTE_USER        (PTE_U | PTE_W | PTE_P)

/* Control Register flags */
#define CR0_PE          0x00000001              // Protection Enable
#define CR0_MP          0x00000002              // Monitor coProcessor
#define CR0_EM          0x00000004              // Emulation
#define CR0_TS          0x00000008              // Task Switched
#define CR0_ET          0x00000010              // Extension Type
#define CR0_NE          0x00000020              // Numeric Errror
#define CR0_WP          0x00010000              // Write Protect
#define CR0_AM          0x00040000              // Alignment Mask
#define CR0_NW          0x20000000              // Not Writethrough
#define CR0_CD          0x40000000              // Cache Disable
#define CR0_PG          0x80000000              // Paging

#define CR4_PCE         0x00000100              // Performance counter enable
#define CR4_MCE         0x00000040              // Machine Check Enable
#define CR4_PSE         0x00000010              // Page Size Extensions
#define CR4_DE          0x00000008              // Debugging Extensions
#define CR4_TSD         0x00000004              // Time Stamp Disable
#define CR4_PVI         0x00000002              // Protected-Mode Virtual Interrupts
#define CR4_VME         0x00000001              // V86 Mode Extensions

#endif /* !__KERN_MM_MMU_H__ */


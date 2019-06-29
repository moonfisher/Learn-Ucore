#ifndef __KERN_TRAP_TRAP_H__
#define __KERN_TRAP_TRAP_H__

#include "defs.h"

/* Trap Numbers */

/* Processor-defined: */
#define T_DIVIDE                0   // divide error
#define T_DEBUG                 1   // debug exception
#define T_NMI                   2   // non-maskable interrupt
#define T_BRKPT                 3   // breakpoint
#define T_OFLOW                 4   // overflow
#define T_BOUND                 5   // bounds check
#define T_ILLOP                 6   // illegal opcode
#define T_DEVICE                7   // device not available
#define T_DBLFLT                8   // double fault
// #define T_COPROC             9   // reserved (not used since 486)
#define T_TSS                   10  // invalid task switch segment
#define T_SEGNP                 11  // segment not present
#define T_STACK                 12  // stack exception
#define T_GPFLT                 13  // general protection fault
#define T_PGFLT                 14  // page fault
// #define T_RES                15  // reserved
#define T_FPERR                 16  // floating point error
#define T_ALIGN                 17  // aligment check
#define T_MCHK                  18  // machine check
#define T_SIMDERR               19  // SIMD floating point error

/* Hardware IRQ numbers. We receive these as (IRQ_OFFSET + IRQ_xx) */
#define IRQ_OFFSET              32  // IRQ 0 corresponds to int IRQ_OFFSET

#define IRQ_TIMER               0
#define IRQ_KBD                 1
#define IRQ_COM1                4
#define IRQ_NIC                 11
#define IRQ_IDE1                14
#define IRQ_IDE2                15
//#define IRQ_NET                 17
#define IRQ_ERROR               19
#define IRQ_SPURIOUS            31

/* *
 * These are arbitrarily chosen, but with care not to overlap
 * processor defined exceptions or interrupt vectors.
 * */
#define T_SWITCH_TOU            120    // user/kernel switch
#define T_SWITCH_TOK            121    // user/kernel switch

// 调用门
#define T_CALLGATE              256

/* registers as pushed by pushal */
struct pushregs
{
    uint32_t reg_edi;
    uint32_t reg_esi;
    uint32_t reg_ebp;
    uint32_t reg_oesp;          /* Useless */
    uint32_t reg_ebx;
    uint32_t reg_edx;
    uint32_t reg_ecx;
    uint32_t reg_eax;
};

// 中断桢 trapframe 很重要，无论是中断，还是进程切换，还是用户态和内核态的切换都是通过更新中断桢来实现的
// 在 __alltraps 函数里通过压栈的方式构造 trapframe 结构给 c 函数传参
// 这里面有很多 padding 字段，是为了占位，__alltraps 通过 pushl 来压栈参数，
// 一次压栈 4 字节，但实际只有 2 字节是有用的
/*
 CF(Carry Flag)：    进位标志位；
 PF(Parity Flag)：   奇偶标志位；
 AF(Assistant Flag)：辅助进位标志位；
 ZF(Zero Flag)：     零标志位；
 SF(Singal Flag)：   符号标志位；
 IF(Interrupt Flag)：中断允许标志位,由CLI，STI两条指令来控制；设置IF位使CPU可识别外部（可屏蔽）中断请求，
                     复位IF位则禁止中断，IF位对不可屏蔽外部中断和故障中断的识别没有任何作用；
 DF(Direction Flag)：向量标志位，由CLD，STD两条指令来控制；
 OF(Overflow Flag)： 溢出标志位；
 IOPL(I/O Privilege Level)：I/O 特权级字段，它的宽度为 2 位,它指定了 I/O 指令的特权级。
                    如果当前的特权级别在数值上小于或等于 IOPL，那么 I/O 指令可执行。否则，将发生一个保护
                    性故障中断；
 NT(Nested Task)：   控制中断返回指令IRET，它宽度为1位。若NT=0，则用堆栈中保存的值恢复EFLAGS，
                    CS和EIP从而实现中断返回；若NT=1，则通过任务切换实现中断返回。在ucore中，设置NT为0。
 */
struct trapframe
{
    struct pushregs tf_regs;    // pushal 压栈的参数
    uint16_t tf_gs;
    uint16_t tf_padding0;
    uint16_t tf_fs;
    uint16_t tf_padding1;
    uint16_t tf_es;
    uint16_t tf_padding2;
    uint16_t tf_ds;
    uint16_t tf_padding3;
    uint32_t tf_trapno;         // 本次中断中断号
    /* below here defined by x86 hardware */
    /* 下面 4 个参数，是在 cpu 执行 int x 中断，跳转到中断函数入口之前，往堆栈里压的数据
       cpu 只负责压栈了 cs，eip，eflags，error code(不是每个中断都有) 这些，其余寄存器
       的当前状态由中断程序根据需要去自行保存并恢复
    */
    uint32_t tf_err;
    uintptr_t tf_eip;
    uint16_t tf_cs;
    uint16_t tf_padding4;
    uint32_t tf_eflags;
    /* below here only when crossing rings, such as from user to kernel */
    /* 下面 2 个参数，记录 int x 中断执行之前的进程用户态堆栈 esp 地址，中断执行完之后还要回到之前的堆栈
       这 2 个只在用户态切换到内核态，发生特权转换的时候才会用到，此时 cpu 会把用户栈切换为内核栈，
       并将当前进程在用户态下的 ss 和 esp 压到新的内核栈中保存起来（因为栈发生切换，如果不保存，中
       断处理完之后无法回到之前的用户栈地址）。内核栈的地址通过全局 tss 获取的，进程切换的时候更新 tss。
       如果是从内核态转向用户态，这个过程并未产生特权级切换，cpu 并未压入对应 ss 和 esp，这 2 个参数没用
    */
    uintptr_t tf_esp;
    uint16_t tf_ss;
    uint16_t tf_padding5;
} __attribute__((packed));

void idt_init(void);
void print_trapframe(struct trapframe *tf);
void print_regs(struct pushregs *regs);
bool trap_in_kernel(struct trapframe *tf);
void trap_init_percpu(void);

#endif /* !__KERN_TRAP_TRAP_H__ */


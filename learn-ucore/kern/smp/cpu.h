
#ifndef CPU_H
#define CPU_H

#include "defs.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

struct rtcdate
{
    uint32_t second;
    uint32_t minute;
    uint32_t hour;
    uint32_t day;
    uint32_t month;
    uint32_t year;
};

// Maximum number of CPUs
#define NCPU    32

// Values of status in struct Cpu
enum
{
    CPU_UNUSED = 0,
    CPU_STARTED,
    CPU_HALTED,
};

// Per-CPU state 是一个用来保存每个 cpu 运行状态的结构体
struct cpu_info
{
    uint8_t cpu_id;                 // index into cpus[] below
    uint8_t apic_id;                // Local APIC ID
    uint8_t acpi_id;                // ACPI ID
    volatile unsigned cpu_status;   // The status of the CPU
    struct segdesc gdt[NSEGS];      // x86 global descriptor table
    struct pseudodesc gdt_pd;       // x86 global descriptor table
    struct taskstate cpu_ts;        // Used by x86 to find stack for interrupt
    struct cpu_info *cpu;           // The currently-running cpu.
    struct proc_struct *proc;       // The currently-running process.
};

// Initialized in mpconfig.c
extern struct cpu_info cpus[NCPU];
extern int ncpu;				    // Total number of CPUs in the system
extern struct cpu_info *bootcpu;    // The boot-strap processor (BSP)
extern physaddr_t lapicaddr;	    // Physical MMIO address of the local APIC
extern physaddr_t ioapicaddr;       // Physical MMIO address of the io APIC
extern uint8_t ioapicid;
extern int ismp;

// Per-CPU kernel stacks 每个 cpu 有自己的栈空间
extern unsigned char percpu_kstacks[NCPU][KSTACKSIZE];

/*
 每个 cpu 如何知道自身的当前运行状态呢？（这个做法类似 linux Per-Cpu 变量原理）
 
 我们可以通过 lapic 获取 cpu 自身编号，再利用编号对 cpus 寻址即可，也就是说，对于任意一个 cpu，
 自身状态的存储位置可以这样获得： C struct cpu *c = &cpus[cpunum()]
 
 然而，我们不可能每次引用 cpu 自身状态时都通过 lapic 先获取编号啊，然后再去遍历 cpus 获取当前 cpu ，
 能不能弄一个全局变量 cpu, 直接指向当前 cpu 在 cpus 中的位置，对于记录每个 cpu 当前正在运行的进程，
 能不能用一个 proc 全局变量，存储当前 cpu 正在运行的进程状态
 
 每个 cpu 是独立并行的，在每个 cpu 上运行的内核代码都是一样的，页表也一样，这意味着全局变量 cpu
 和 proc 的地址也是一样的，我们需要一种方法，可以让我们在每个 cpu 中都用同一个变量来记录状态，
 但这些变量却能映射到不同的虚拟地址。既然页表一样，我们自然不能用一个绝对的数值来寻址啦，页表是相同的，
 但段表却可以不同。
 
 所以我们需要用 segment register 来寻址，只要我们在建立段表时把该段都映射到不同的内存区域不就可以了，
 所以我们有了以下声明，用 gs 作为段寄存器，cpu 指向 [%gs]，proc 指向 [%gs + 4]
*/
#if ASM_NO_64
    extern struct cpu_info *current_cpu asm("%gs:0");       // &cpus[cpunum()]
    extern struct proc_struct *current_proc asm("%gs:4");   // cpus[cpunum()].proc
#else
    struct cpu_info *current_cpu;                           // &cpus[cpunum()]
    struct proc_struct *current_proc;                       // cpus[cpunum()].proc
#endif

int cpunum(void);
#define thiscpu (&cpus[cpunum()])

void mp_init(void);
void lapic_init(void);
void lapic_startap(uint8_t apicid, uint32_t addr);
void lapic_eoi(void);
void lapic_ipi(int vector);
void ioapic_enable(int irq, int cpunum);
void cmos_time(struct rtcdate *r);

#endif

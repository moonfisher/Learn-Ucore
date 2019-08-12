#include "defs.h"
#include "mmu.h"
#include "memlayout.h"
#include "clock.h"
#include "trap.h"
#include "x86.h"
#include "stdio.h"
#include "assert.h"
#include "console.h"
#include "vmm.h"
#include "swap.h"
#include "kdebug.h"
#include "unistd.h"
#include "syscall.h"
#include "error.h"
#include "sched.h"
#include "sync.h"
#include "proc.h"
#include "string.h"
#include "cpu.h"
#include "spinlock.h"

#define TICK_NUM        5000
#define T_TASKGATE      0x90

static void print_ticks()
{
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* *
 * Interrupt descriptor table:
 *
 * Must be built at run time because shifted function addresses can't
 * be represented in relocation records.
 * */
static struct gatedesc idt[256] = {{0}};    // 0xC0158780

/* 0xC0155560
{
    pd_lim = 0x7FF,
    pd_base = 0xC0158780
}
*/
#if ASM_NO_64
    static struct pseudodesc idt_pd = {sizeof(idt) - 1, (uintptr_t)idt};
#else
    static struct pseudodesc idt_pd;
#endif

// Initialize and load the per-CPU TSS and IDT
void trap_init_percpu(void)
{
    // The example code here sets up the Task State Segment (TSS) and
    // the TSS descriptor for CPU 0. But it is incorrect if we are
    // running on other CPUs because each CPU has its own kernel stack.
    // Fix the code so that it works for all CPUs.
    //
    // Hints:
    //   - The macro "thiscpu" always refers to the current CPU's
    //     struct CpuInfo;
    //   - The ID of the current CPU is given by cpunum() or
    //     thiscpu->cpu_id;
    //   - Use "thiscpu->cpu_ts" as the TSS for the current CPU,
    //     rather than the global "ts" variable;
    //   - Use gdt[(GD_TSS0 >> 3) + i] for CPU i's TSS descriptor;
    //   - You mapped the per-CPU kernel stacks in mem_init_mp()
    //   - Initialize cpu_ts.ts_iomb to prevent unauthorized environments
    //     from doing IO (0 is not the correct value!)
    //
    // ltr sets a 'busy' flag in the TSS selector, so if you
    // accidentally load the same TSS on more than one CPU, you'll
    // get a triple fault.  If you set up an individual CPU's TSS
    // wrong, you may not get a fault until you try to return from
    // user space on that CPU.
    //
    struct cpu_info *cpu = thiscpu;
    int cid = cpu->cpu_id;
    // Setup a TSS so that we get the right stack
    // when we trap to the kernel.
    cpu->cpu_ts.ts_esp0 = (uintptr_t)percpu_kstacks[cid];
    cpu->cpu_ts.ts_ss0 = KERNEL_DS;
    cpu->cpu_ts.ts_iomb = sizeof(struct taskstate); //修复这里的一个bug
    
    // Initialize the TSS slot of the gdt.
    // 之前初始化了一个全局 gdt，但这里需要修改 gdt，因此每个 cpu 都维护一个 gdt 结构
    extern struct segdesc gdt[NSEGS];
    memcpy(cpu->gdt, gdt, sizeof(gdt));
    
    cpu->gdt_pd.pd_base = (uintptr_t)(cpu->gdt);
    cpu->gdt_pd.pd_lim = sizeof(cpu->gdt) - 1;
    
    cpu->gdt[SEG_TSS] = SEG16(STS_T32A, (uint32_t)(&(cpu->cpu_ts)), sizeof(struct taskstate), 0);
    cpu->gdt[SEG_TSS].sd_s = 0;
    
    // Map cpu, and curproc
    // 不同 cpu 所在的 SEG_KCPU 对应的段基地址不同，这里段基地址都设置为当前 cpu info 地址
    cpu->gdt[SEG_KCPU] = SEG16(STA_W, (uint32_t)(&(cpu->cpu)), 2 * sizeof(uint32_t), 0);
    
    // 设置 GDT，每个 CPU 都需要执行一次
    lgdt(&(cpu->gdt_pd));
    
    // 加载 gs 寄存器，fs / gs 寄存器可以用来做一些和 cpu 相关的本地存储
    loadgs(GD_KCPU);
    
    // Load the IDT
    lidt(&idt_pd);
        
    // Load the TSS selector (like other segment selectors, the
    // bottom three bits are special; we leave them 0)
    ltr(GD_TSS);
    
    // 这里看起来每个 CPU 都是在给同一个 cpu 和 proc 变量赋值，好像会覆盖
    // 但实际上因为 gs 段表映射不同，cpu 和 proc 这 2 个变量的虚拟地址是不同的
    current_cpu = cpu;
    current_proc = 0;
}

/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S */
/*
 IDT 表中只可以安装 3 种门：中断门、陷阱门和任务门
 在执行 INT 指令时，实际完成了以下几条操作：
（1） 由于 INT 指令发生了不同优先级之间的控制转移，所以首先从 TSS（任务状态段）中获取
     高优先级的内核堆栈信息（SS 和 ESP）
（2） 把低优先级堆栈信息（SS 和 ESP）保留到高优先级堆栈（即内核栈）中
（3） 把 EFLAGS，外层CS，EIP推入高优先级堆栈（内核栈）中
（4） 通过 IDT 加载 CS，EIP（控制转移至中断处理函数），所以进入 __vectors 中断函数时，cs 已经修改过了
*/
void idt_init(void)
{
#if ASM_NO_64
    extern uintptr_t __vectors[];
#else
    uintptr_t __vectors[1];
#endif
    int i = 0;
    // 中断门
    for (i = 0; i < sizeof(idt) / sizeof(struct gatedesc); i ++)
    {
        idt[i].gd_off_15_0 = (uint32_t)(__vectors[i]) & 0xffff;
        idt[i].gd_ss = GD_KTEXT;    // 段选择子指向内核段，中断之后特权级提升，堆栈切换
        idt[i].gd_args = 0;         // 这里是 0，只有调用门才用得上这个
        idt[i].gd_rsv1 = 0;
        idt[i].gd_type = STS_IG32;  // 这是中断门
        idt[i].gd_s = 0;            // 中断是系统段，这里必须为 0
        idt[i].gd_dpl = DPL_KERNEL;
        idt[i].gd_p = 1;
        idt[i].gd_off_31_16 = (uint32_t)(__vectors[i]) >> 16;
    }
    
    /*
     中断门与陷阱门在使用上的区别
     并不在于中断是由外部产生的或是由 cpu 本身产生的，而是在于通过中断门进入中断服务程序时 cpu
     会自动将中断关闭，也就是将 cpu 中 eflags 寄存器中 IF 标志复位至 0，防止嵌套中断的发生
     而通过陷阱门进入服务程序时则维持 IF 标志不变
    */
    // T_SYSCALL = int 0x80，80 中断设置的权限是 DPL_USER
    // 这是用户进程可以切换到内核态，执行内核代码的唯一入口，如果改成 DPL_KERNEL 则
    // 会触发 T_GPFLT general protection fault 中断，导致异常
    idt[T_SYSCALL].gd_off_15_0 = (uint32_t)(__vectors[T_SYSCALL]) & 0xffff;
    idt[T_SYSCALL].gd_ss = GD_KTEXT;    // 段选择子指向内核段，中断之后特权级提升，堆栈切换
    idt[T_SYSCALL].gd_args = 0;         // 这里是 0，只有调用门才用得上这个
    idt[T_SYSCALL].gd_rsv1 = 0;
    idt[T_SYSCALL].gd_type = STS_TG32;  // 这是陷阱门，系统调动是通过陷阱门来实现
    idt[T_SYSCALL].gd_s = 0;            // 中断是系统段，这里必须为 0
    idt[T_SYSCALL].gd_dpl = DPL_USER;   // 陷阱门是提供给用户进程使用的，这里要设置为 DPL_USER
    idt[T_SYSCALL].gd_p = 1;
    idt[T_SYSCALL].gd_off_31_16 = (uint32_t)(__vectors[T_SYSCALL]) >> 16;
    
    // 任务门
    extern struct taskstate taskgate;
    taskgate.ts_esp0 = 0xc06df000;
    taskgate.ts_ss0 = KERNEL_DS;
    taskgate.ts_ebp = 0xafffffc4;
    taskgate.ts_esp = 0xafffff98;
    taskgate.ts_ss = USER_DS;
    taskgate.ts_ds = USER_DS;
    taskgate.ts_es = USER_DS;
    taskgate.ts_fs = 0x0;
    taskgate.ts_gs = 0x0;
    taskgate.ts_eax = 0x2;
    taskgate.ts_ebx = 0x1;
    taskgate.ts_ecx = 0x1;
    taskgate.ts_edx = 0x8023b4;
    taskgate.ts_esi = 0x0;
    taskgate.ts_edi = 0x0;
    taskgate.ts_cs = USER_CS;
    taskgate.ts_eip = 0x802048;
    taskgate.ts_cr3 = 0x6e1000;
    taskgate.ts_eflags = 0x282;
    
    idt[T_TASKGATE].gd_off_15_0 = 0;
    idt[T_TASKGATE].gd_ss = GD_TASK_GATE;   // 段选择子指向内核段，中断之后特权级提升，堆栈切换
    idt[T_TASKGATE].gd_args = 0;
    idt[T_TASKGATE].gd_rsv1 = 0;
    idt[T_TASKGATE].gd_type = STS_TG;       // 这是任务门
    idt[T_TASKGATE].gd_s = 0;               // 任务门是系统段，这里必须为 0
    idt[T_TASKGATE].gd_dpl = DPL_KERNEL;    // 陷阱门是提供给用户进程使用的，要设置为 DPL_USER
    idt[T_TASKGATE].gd_p = 1;
    idt[T_TASKGATE].gd_off_31_16 = 0;

    trap_init_percpu();
}

static const char *trapname(int trapno)
{
    static const char * const excnames[] = {
        "Divide error",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "BOUND Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection",
        "Page Fault",
        "(unknown trap)",
        "x87 FPU Floating-Point Error",
        "Alignment Check",
        "Machine-Check",
        "SIMD Floating-Point Exception"
    };

    if (trapno < sizeof(excnames) / sizeof(const char * const))
    {
        return excnames[trapno];
    }
    
    if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
    {
        return "Hardware Interrupt";
    }
    
    if (trapno == T_SYSCALL)
    {
        return "System Call";
    }
    
    if (trapno == T_CALLGATE)
    {
        return "Call Gate";
    }
    
    return "(unknown trap)";
}

/* trap_in_kernel - test if trap happened in kernel */
// 判断中断发生时，CPU 压入堆栈的 cs 段描述符是否指向内核代码段，
// 是则说明是在执行内核进程的时候发生了中断，否则是执行用户进程时发生中断
bool trap_in_kernel(struct trapframe *tf)
{
    return (tf->tf_cs == (uint16_t)KERNEL_CS);
}

static const char *IA32flags[] = {
    "CF", NULL, "PF", NULL, "AF", NULL, "ZF", "SF",
    "TF", "IF", "DF", "OF", NULL, NULL, "NT", NULL,
    "RF", "VM", "AC", "VIF", "VIP", "ID", NULL, NULL,
};

void print_trapframe(struct trapframe *tf)
{
    int cid = thiscpu->cpu_id;
    cprintf("\ntrapframe at cpu:%d, %p\n", cid, tf);
    
    char *csDes = "Trap from User";
    if (tf->tf_cs == (uint16_t)KERNEL_CS)
    {
        csDes = "Trap from Kernel";
    }
    
    print_regs(&(tf->tf_regs));
    cprintf("  ds       0x%08x\n", tf->tf_ds);
    cprintf("  es       0x%08x\n", tf->tf_es);
    cprintf("  fs       0x%08x\n", tf->tf_fs);
    cprintf("  gs       0x%08x\n", tf->tf_gs);
    cprintf("  trapno   0x%08x  %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
    cprintf("  err      0x%08x\n", tf->tf_err);
    cprintf("  eip      0x%08x\n", tf->tf_eip);
    cprintf("  cs       0x%08x  %s\n", tf->tf_cs, csDes);
    cprintf("  flag     0x%08x ", tf->tf_eflags);

    int i, j;
    for (i = 0, j = 1; i < sizeof(IA32flags) / sizeof(IA32flags[0]); i ++, j <<= 1)
    {
        if ((tf->tf_eflags & j) && IA32flags[i] != NULL)
        {
            cprintf(" %s, ", IA32flags[i]);
        }
    }
    cprintf("IOPL = %d\n", (tf->tf_eflags & FL_IOPL_MASK) >> 12);

//    if (!trap_in_kernel(tf))
    {
        cprintf("  esp      0x%08x\n", tf->tf_esp);
        cprintf("  ss       0x%08x\n", tf->tf_ss);
    }
    cprintf("\n");
}

void print_regs(struct pushregs *regs)
{
    cprintf("  edi      0x%08x\n", regs->reg_edi);
    cprintf("  esi      0x%08x\n", regs->reg_esi);
    cprintf("  ebp      0x%08x\n", regs->reg_ebp);
    cprintf("  oesp     0x%08x\n", regs->reg_oesp);
    cprintf("  ebx      0x%08x\n", regs->reg_ebx);
    cprintf("  edx      0x%08x\n", regs->reg_edx);
    cprintf("  ecx      0x%08x\n", regs->reg_ecx);
    cprintf("  eax      0x%08x\n", regs->reg_eax);
}

static inline void print_pgfault(struct trapframe *tf)
{
    /* error_code:
     * bit 0 == 0 means no page found, 1 means protection fault
     * bit 1 == 0 means read, 1 means write
     * bit 2 == 0 means kernel, 1 means user
     * */
    cprintf("page fault at 0x%08x: %c/%c [%s].\n", rcr2(),
            (tf->tf_err & 4) ? 'U' : 'K',
            (tf->tf_err & 2) ? 'W' : 'R',
            (tf->tf_err & 1) ? "protection fault" : "no page found");
}

static int pgfault_handler(struct trapframe *tf)
{
    extern struct mm_struct *check_mm_struct;
    if (check_mm_struct != NULL)
    {
        //used for test check_swap
        print_pgfault(tf);
    }
    
    struct mm_struct *mm;
    if (check_mm_struct != NULL)
    {
        assert(current == idleproc);
        mm = check_mm_struct;
    }
    else
    {
        if (current == NULL)
        {
            print_trapframe(tf);
            print_pgfault(tf);
            panic("unhandled page fault.\n");
        }
        mm = current->mm;
    }
    
    return do_pgfault(mm, tf->tf_err, rcr2());
}

//static volatile int in_swap_tick_event = 0;
extern struct mm_struct *check_mm_struct;

/* temporary trapframe or pointer to trapframe */
struct trapframe switchk2u, *switchu2k;

static void trap_dispatch(struct trapframe *tf)
{
    char c;
    int ret = 0;

    switch (tf->tf_trapno)
    {
        case T_PGFLT:  //page fault
            if ((ret = pgfault_handler(tf)) != 0)
            {
                print_trapframe(tf);
                if (current == NULL)
                {
                    panic("handle pgfault failed. ret=%d\n", ret);
                }
                else
                {
                    if (trap_in_kernel(tf))
                    {
                        // 内核的地址空间是一开始都规划好，也分配好物理页面了，不可能发生缺页中断
                        // 也不可能用到 swap 交换分区，否则内核就有严重问题
                        panic("handle pgfault failed in kernel mode. ret=%d\n", ret);
                    }
                    cprintf("killed by kernel.\n");
                    //panic("handle user mode pgfault failed. ret=%d\n", ret);
                    do_exit(-E_KILLED);
                }
            }
            break;
            
        case T_SYSCALL:
            syscall();
            break;

        case IRQ_OFFSET + IRQ_TIMER:
            // 每个 cpu 都启动了 local apic 定时器，
            // 但只能一个 cpu 来处理计时，否则乱了
            if (thiscpu->cpu_id == 0)
            {
                ticks++;
                assert(current != NULL);
                run_timer_list();
                if (ticks % TICK_NUM == 0)
                {
                    print_ticks();
                }
            }
            lapic_eoi();
            break;
            
        case IRQ_OFFSET + IRQ_NIC:
            cprintf("nic irq\n");
            lapic_eoi();
            break;

        case IRQ_OFFSET + IRQ_COM1:
            c = cons_getc();
            cprintf("serial [%03d] %c from cpu %d\n", c, c, thiscpu->cpu_id);
            extern void dev_stdin_write(char c);
            dev_stdin_write(c);
            lapic_eoi();
            break;
            
        case IRQ_OFFSET + IRQ_KBD:
            c = cons_getc();
            cprintf("kbd [%03d] %c from cpu %d\n", c, c, thiscpu->cpu_id);
            extern void dev_stdin_write(char c);
            dev_stdin_write(c);
            lapic_eoi();
            break;
            
        case IRQ_OFFSET + IRQ_IDE1:
        case IRQ_OFFSET + IRQ_IDE2:
            /* do nothing */
//            print_trapframe(tf);
            lapic_eoi();
            break;
            
        case T_SWITCH_TOU:
            if (tf->tf_cs != USER_CS)
            {
                // 当前在内核态，需要建立切换到用户态所需要的 trapframe，通过中断返回切换到用户态
                // 这里只是模拟进入用户态，真正内核初始化完是需要运行一个用户进程来进入用户态
                // 但实际上最后本质都是通过中断，构造中断桢来修改 cpu 寄存器来实现状态切换
                switchk2u = *tf;
                switchk2u.tf_cs = USER_CS;
                switchk2u.tf_ds = switchk2u.tf_es = switchk2u.tf_ss = USER_DS;
                
                // 如果是通过执行用户进程切换到用户态，这里 tf_esp 应该指向用户进程空间的堆栈地址，堆栈切换
                // 这里只是模拟，把内核代码切换到用户态，所以直接找到 int x 中断执行之前的 esp 地址，
                // 中断执行完之后还是直接用之前的内核堆栈，不切换堆栈
                switchk2u.tf_esp = (uint32_t)tf + sizeof(struct trapframe) - 8;
                
                // set eflags, make sure ucore can use io under user mode.
                // if CPL > IOPL, then cpu will generate a general protection.
                // 设置 EFLAG 的 I/O 特权位，使得在用户态可使用 in/out 指令
                switchk2u.tf_eflags |= FL_IOPL_MASK;
                
                // set temporary stack
                // then iret will jump to the right stack
                // 设置临时栈，指向 switchk2u，这样当 trap() 返回时，cpu 会从 switchk2u 恢复数据，
                // 而不是从现有 tf 栈恢复数据，相当于用 switchk2u 替换了 tf 的内容。
                // tf - 1 = pushl %esp
                *((uint32_t *)tf - 1) = (uint32_t)&switchk2u;
            }
            break;
            
        case T_SWITCH_TOK:
            if (tf->tf_cs != KERNEL_CS)
            {
                // 发出中断时，cpu 处于用户态，我们希望处理完中断后，cpu 能运行在内核态
                // 所以把 cs 和 ds 都设置为内核代码段和内核数据段
                tf->tf_cs = KERNEL_CS;
                tf->tf_ds = tf->tf_es = KERNEL_DS;
                tf->tf_eflags &= ~FL_IOPL_MASK;
                
                // 从用户态切换到内核态时，发生特权变更，cpu 已经根据 tss 内容把堆栈从用户空间堆栈
                // 切换到了内核堆栈，tf 结构现在在内核栈空间，但 tf_esp 还是指向 int x 执行之前的
                // 用户堆栈栈顶 ，现在需要在用户栈空间也构造 trapframe，所以把 tf 的内容先修改下
                // 然后复制到用户栈空间里，再返回让 cpu 从用户栈空间恢复数据来实现切换状态
                switchu2k = (struct trapframe *)(tf->tf_esp - (sizeof(struct trapframe) - 8));
                memmove(switchu2k, tf, sizeof(struct trapframe) - 8);
                *((uint32_t *)tf - 1) = (uint32_t)switchu2k;
            }
            break;
            
        default:
            print_trapframe(tf);
            if (current != NULL)
            {
                cprintf("unhandled trap.\n");
                do_exit(-E_KILLED);
            }
            // in kernel, it must be a mistake
            panic("unexpected trap in kernel.\n");
    }
}

/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap() returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
/*
 Linux 用户抢占和内核抢占
 https://blog.csdn.net/gatieme/article/details/51872618
 https://blog.csdn.net/a2796749/article/details/47101595
*/
// 参数 tf 在汇编函数 __alltraps 里设置好，再调用 c 函数 trap
void trap(struct trapframe *tf)
{
    // current 为空说明当前还未创建进程就收到了中断，直接处理就行
    if (current == NULL)
    {
        trap_dispatch(tf);
    }
    else
    {
        // keep a trapframe chain in stack
        // 先保存中断处理之前，current 关联的 tf
        struct trapframe *otf = current->tf;
        current->tf = tf;
    
        bool in_kernel = trap_in_kernel(tf);
    
        // 中断处理过程有可能会修改 tf 的内容
        trap_dispatch(tf);
    
        current->tf = otf;
        
        /*
         这里表明了只有当进程在 用户态 执行到 “任意” 某处用户代码位置时发生了中断，
         且当前进程控制块成员变量 need_resched 为 1（表示需要调度了）时，才会执行 shedule 函数。
         这实际上体现了对用户进程的可抢占性。
         
         如果内核处于相对耗时的操作中, 比如文件系统或者内存管理相关的任务, 这种行为可能会带来问题.
         这种情况下, 内核代替特定的进程执行相当长的时间, 而其他进程无法执行, 无法调度, 这就造成了
         系统的延迟增加, 用户体验到”缓慢”的响应. 比如如果多媒体应用长时间无法得到 CPU, 则可能发生
         视频和音频漏失现象.

         如果没有第一行的 if 语句，那么就可以体现对内核代码的可抢占性。但如果要把这一行 if 语句去掉，
         我们就不得不实现对 ucore 中的所有全局变量的互斥访问操作，以防止所谓的 race condition 现象，
         这样 ucore 的实现复杂度会增加不少，为了简单，ucore 并没有实现内核抢占，当用户进程从用户态
         陷入内核态之后，在内核处理完相关中断，再次返回到用户态之前，是不会被 schedule 调度的
         */
        if (!in_kernel)
        {
            // 如果一个进程被另外的进程 kill 掉，这个进程并不会马上销毁，只是给他设置上 PF_EXITING 标记
            // 等到下次中断的时候，检测当前进程是否被 kill 过，再进行清理
            if (current->flags & PF_EXITING)
            {
                do_exit(-E_KILLED);
            }
            
            /*
             进入这个分支，说明之前是从用户态切换到内核态，那么当内核态即将返回用户态时, 内核会检查
             need_resched 是否设置, 如果设置, 则调用 schedule()，此时，发生用户抢占.
             用户态切入内核态之后，有可能引起重新调度，比如基于时间片的计算，时间片达到最大之后，重新调度
             正是因为在中断结束的时候，设置这个检测点，同时不间断，周期性的时钟中断，操作系统内核才能一直
             有机会获取代码执行权，否则用户进程写个死循环就完了
            */
            if (current->need_resched)
            {
                schedule();
            }
        }
    }
}


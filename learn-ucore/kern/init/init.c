#include "defs.h"
#include "stdio.h"
#include "string.h"
#include "console.h"
#include "kdebug.h"
#include "picirq.h"
#include "trap.h"
#include "clock.h"
#include "intr.h"
#include "pmm.h"
#include "vmm.h"
#include "ide.h"
#include "swap.h"
#include "proc.h"
#include "fs.h"
#include "sync.h"
#include "sockets.h"
#include "net.h"
#include "netcheck.h"
#include "cpu.h"

int kern_init(void) __attribute__((noreturn));
int mon_backtrace(int argc, char **argv, struct trapframe *tf);
void boot_aps(void);

//static void lab1_switch_test(void);
void grade_backtrace(void);

int main(void) {}

int kern_init(void)
{
    // 对于位于BSS段中未初始化的全局变量，需要初始化为0，确保代码能正确执行
#if ASM_NO_64
    extern char edata[], end[];
#else
    char edata[1], end[1];
#endif
    memset(edata, 0, (size_t)(end - edata));

    cons_init();                // init the console

    const char *message = "(THU.CST) os is loading ...";
    cprintf("%s\n\n", message);

    print_kerninfo();
    grade_backtrace();

    pmm_init();                 // init physical memory management

    mp_init();                  // init cpus apic
    lapic_init();               // init local apic

    // Acquire the big kernel lock before waking up APs
    // Your code here:
//    lock_kernel(); //BSP获取内核锁
    // Starting non-boot CPUs
//    boot_aps(); //将初始化代码拷贝到MPENTRY_PADDR处，然后依次启动所有AP

    pic_init();                 // init interrupt controller
    idt_init();                 // init interrupt descriptor table

    vmm_init();                 // init virtual memory management
    sched_init();               // init scheduler
    sync_init();
    
    proc_init();                // init process table

    ide_init();                 // init ide devices
    swap_init();                // init swap
    fs_init();                  // init fs
    
    clock_init();               // init clock interrupt
    
    net_init();                  // init nic
    net_check();
    
    intr_enable();              // enable irq interrupt
    cpu_idle();                 // run idle process
}

// While boot_aps is booting a given CPU, it communicates the per-core
// stack pointer that should be loaded by mpentry.S to that CPU in
// this variable.
void *mpentry_kstack;

// Start the non-boot (AP) processors.
void boot_aps(void)
{
#if ASM_NO_64
    extern unsigned char mpentry_start[], mpentry_end[];
#else
    unsigned char mpentry_start[1], mpentry_end[1];
#endif
    void *code;
    struct cpu_info *c;
    
    // Write entry code to unused memory at MPENTRY_PADDR
    code = KADDR(MPENTRY_PADDR);
    memmove(code, mpentry_start, mpentry_end - mpentry_start);
    
    // Boot each AP one at a time
    for (c = cpus; c < cpus + ncpu; c++)
    {
        if (c == cpus + cpunum()) // We've started already. 现在运行在BSP
            continue;
        
        // Tell mpentry.S what stack to use
        mpentry_kstack = percpu_kstacks[c - cpus] + KSTACKSIZE;
        // Start the CPU at mpentry_start
        lapic_startap(c->cpu_id, PADDR(code));
        // Wait for the CPU to finish some basic setup in mp_main()
        while (c->cpu_status != CPU_STARTED)
        {
            ;
        }
    }
}

// Setup code for APs
void mp_main(void)
{
    // We are in high EIP now, safe to switch to kern_pgdir
    lcr3(boot_cr3);
    cprintf("SMP: CPU %d starting\n", cpunum());
    
    lapic_init();
//    env_init_percpu();                         //设置GDT，每个CPU都需要执行一次
//    trap_init_percpu();                         //安装TSS描述符，每个CPU都需要执行一次
    xchg(&thiscpu->cpu_status, CPU_STARTED); // tell boot_aps() we're up，需要原子操作
    
    // Now that we have finished some basic setup, call sched_yield()
    // to start running processes on this CPU.  But make sure that
    // only one CPU can enter the scheduler at a time!
    //
    // Your code here:
//    lock_kernel();
    schedule();
}

void __attribute__((noinline)) grade_backtrace2(int arg0, int arg1, int arg2, int arg3)
{
    mon_backtrace(0, NULL, NULL);
}

void __attribute__((noinline)) grade_backtrace1(int arg0, int arg1)
{
    grade_backtrace2(arg0, (int)&arg0, arg1, (int)&arg1);
}

void __attribute__((noinline)) grade_backtrace0(int arg0, int arg1, int arg2)
{
    grade_backtrace1(arg0, arg2);
}

void grade_backtrace(void)
{
    grade_backtrace0(0, (int)kern_init, 0xffff0000);
}

//static void lab1_print_cur_status(void)
//{
//    static int round = 0;
//    uint16_t reg1, reg2, reg3, reg4;
//    asm volatile (
//            "mov %%cs, %0;"
//            "mov %%ds, %1;"
//            "mov %%es, %2;"
//            "mov %%ss, %3;"
//            : "=m"(reg1), "=m"(reg2), "=m"(reg3), "=m"(reg4));
//    cprintf("%d: @ring %d\n", round, reg1 & 3);
//    cprintf("%d:  cs = %x\n", round, reg1);
//    cprintf("%d:  ds = %x\n", round, reg2);
//    cprintf("%d:  es = %x\n", round, reg3);
//    cprintf("%d:  ss = %x\n", round, reg4);
//    round ++;
//}

//static void lab1_switch_to_user(void)
//{
//    /*
//     转向用户态时，我们需要预留出8个字节来存放 iret 的返回，在调用中断之前先修改 esp，
//     原因是切换特权级时，iret 指令会额外弹出 ss 和 esp，但实际在从内核态转向用户态的时候，
//     调用中断时并未产生特权级切换，只有从用户态进入内核态才叫做特权切换，因此当前场景下并未
//     压入对应 ss 和 esp。这里为了在数据结构上保持一致，先预留空间，假设 cpu 也压入了 ss 和 esp
//     这样从上层代码逻辑来看，数据结构是统一的。
//     */
//    asm volatile (
//        "sub $0x8, %%esp \n"
//        "int %0 \n"
//        "movl %%ebp, %%esp"
//        :
//        : "i"(T_SWITCH_TOU)
//    );
//}

//static void lab1_switch_to_kernel(void)
//{
//    /*
//     转向内核态时，cpu 会将当前程序使用的用户态的 ss 和 esp 压到新的内核栈中保存起来。
//     内核栈的地址通过 tss 获取
//     */
//    asm volatile (
//        "int %0 \n"
//        "movl %%ebp, %%esp \n"
//        :
//        : "i"(T_SWITCH_TOK)
//    );
//}

//static void lab1_switch_test(void)
//{
//    lab1_print_cur_status();
//    cprintf("+++ switch to  user  mode +++\n");
//    lab1_switch_to_user();
//    lab1_print_cur_status();
//    cprintf("+++ switch to kernel mode +++\n");
//    lab1_switch_to_kernel();
//    lab1_print_cur_status();
//}


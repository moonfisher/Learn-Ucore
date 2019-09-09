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
#include "spinlock.h"
#include "acpi.h"
#include "pci.h"

int kern_init(void) __attribute__((noreturn));
int mon_backtrace(int argc, char **argv, struct trapframe *tf);
void boot_aps(void);
void ioapic_init(void);
//static void lab1_switch_test(void);
void grade_backtrace(void);
int main(void) {}

/*
 * validate_cpuid()
 * returns on success, quietly exits on failure (make verbose with -v)
 * cpuid 指令详情 https://en.wikipedia.org/wiki/CPUID
 */
void validate_cpuid(void)
{
    unsigned int eax, ebx, ecx, edx, max_level;
    unsigned int fms, family, model, stepping;
    
    eax = ebx = ecx = edx = 0;
    
    asm("cpuid" : "=a" (max_level), "=b" (ebx), "=c" (ecx), "=d" (edx) : "a" (0));
    
    if (ebx != 0x756e6547 || edx != 0x49656e69 || ecx != 0x6c65746e)
    {
        cprintf("%.4s%.4s%.4s != GenuineIntel", (char *)&ebx, (char *)&edx, (char *)&ecx);
        return;
    }
    
    asm("cpuid" : "=a" (fms), "=c" (ecx), "=d" (edx) : "a" (1) : "ebx");
    family = (fms >> 8) & 0xf;
    model = (fms >> 4) & 0xf;
    stepping = fms & 0xf;
    if (family == 6 || family == 0xf)
        model += ((fms >> 16) & 0xf) << 4;
    
    cprintf("CPUID %d levels family : model : stepping " "0x%x : %x : %x (%d : %d : %d)\n", max_level, family, model, stepping, family, model, stepping);
    
    if (!(edx & (1 << 5)))
    {
        cprintf("CPUID: no MSR\n");
    }
    
    /*
     * Support for MSR_IA32_ENERGY_PERF_BIAS
     * is indicated by CPUID.06H.ECX.bit3
     */
    asm("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) : "a" (6));
    cprintf("CPUID.06H.ECX: 0x%x\n", ecx);
    
    if (!(ecx & (1 << 3)))
    {
        cprintf("CPUID: No MSR_IA32_ENERGY_PERF_BIAS\n");
    }
    
    if (cpu_has_feature(X86_FEATURE_APIC))
    {
        cprintf("CPUID: cpu has apic feature\n");
    }
    
    return;    /* success */
}

int kern_init(void)
{
    // 对于位于 BSS 段中未初始化的全局变量，需要初始化为 0，确保代码能正确执行
#if ASM_NO_64
    extern char edata[], end[];
#else
    char edata[1], end[1];
#endif
    memset(edata, 0, (size_t)(end - edata));

    // 这里要先初始化输出，否则后续的 printf 无法输出日志
    cons_early_init();          // init the console

    cprintf("==================================\n");
    struct rtcdate date;
    cmos_time(&date);
    const char *message = "UCore os is loading ...";
    cprintf("%s at %d-%d-%d %d:%d:%d\n", message, date.year, date.month, date.day, date.hour, date.minute, date.second);
    cprintf("==================================\n");
    
    validate_cpuid();
    print_kerninfo();
    grade_backtrace();

    pmm_init();                 // init physical memory management

    if (acpi_init())            // try to use acpi for machine info
        mp_init();              // otherwise use bios MP tables

    lapic_init();               // init smp local apic
    
    // 启动 smp 之后，8259A 的中断方式将不再用，改为 ioapic 方式
    pic_init();                 // init 8259A interrupt controller
    ioapic_init();              // init ioapic interrupt controller
    
    cons_init();

    idt_init();                 // init interrupt descriptor table

    vmm_init();                 // init virtual memory management
    sched_init();               // init scheduler
    sync_init();
    
    proc_init();                // init process table

    ide_init();                 // init ide devices
    swap_init();                // init swap
    fs_init();                  // init fs
    
    // 启动 smp 之后，定时器也改为使用 local apic 实现
    if (!ismp)
        clock_init();           // init clock interrupt
    
    pci_init();                 // init pci
    
    lock_kernel();              // smp acquire big kernel lock before waking up APs
#if SMP
    boot_aps();                 // smp starting non-boot CPUs
#endif
    intr_enable();              // enable irq interrupt
    unlock_kernel();
    
    cpu_idle();                 // run idle process
}

// While boot_aps is booting a given CPU, it communicates the per-core
// stack pointer that should be loaded by mpentry.S to that CPU in
// this variable.
void *mpentry_kstack;

// Start the non-boot (AP) processors.
// https://www.jianshu.com/p/fc9a8572a830
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
    /*
     我们将 AP 入口代码拷贝到 0x7000(MPENTRY_PADDR)，这个地址没人使用
     当然其实你拷贝到 640KB 之下的任何可用的按页对齐的物理地址都是可以的。
    */
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
        cprintf("boot_aps, start up cpu:%x, stack:%x\n", c->cpu_id, mpentry_kstack);
        
        // 这里启动 ap 用的是 cpu_id 数组索引，而不是真正的 apic_id，是因为不同 CPU 的
        // apic_id 不是连续的数字，不便于 cpus 数组索引
//        lapic_startap(c->cpu_id, PADDR(code));
        lapic_startap(c->apic_id, PADDR(code));
        
        // Wait for the CPU to finish some basic setup in mp_main()
        // 这里是循环同步等待
        while (c->cpu_status != CPU_STARTED)
        {
            ;
        }
    }
}

// Setup code for APs
// AP CPU 初始化入口
void mp_main(void)
{
    // We are in high EIP now, safe to switch to kern_pgdir
    lcr3(boot_cr3);
    cprintf("SMP: mp_main CPU %d starting\n", cpunum());
    
    lapic_init();

    //安装 TSS 描述符，每个 CPU 都需要执行一次
    trap_init_percpu();
    
    // tell boot_aps() we're up，需要原子操作
    xchg(&thiscpu->cpu_status, CPU_STARTED);
    
    struct cpu_info *cpu = current_cpu;
    cprintf("SMP: mp_main CPU %d started\n", cpu->cpu_id);
    
    // Now that we have finished some basic setup, call sched_yield()
    // to start running processes on this CPU.  But make sure that
    // only one CPU can enter the scheduler at a time!
    //
    lock_kernel();
//    intr_enable();
    
//    schedule();
    while (1) {
        ;
    }
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


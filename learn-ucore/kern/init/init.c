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

int kern_init(void) __attribute__((noreturn));
int mon_backtrace(int argc, char **argv, struct trapframe *tf);

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

    pic_init();                 // init interrupt controller
    idt_init();                 // init interrupt descriptor table

    vmm_init();                 // init virtual memory management
    sched_init();               // init scheduler
    proc_init();                // init process table

    ide_init();                 // init ide devices
    swap_init();                // init swap
    fs_init();                  // init fs
    
    clock_init();               // init clock interrupt
    intr_enable();              // enable irq interrupt

    cpu_idle();                 // run idle process
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


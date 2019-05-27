#include "stdio.h"
#include "ulib.h"

void test_call_user_func(void)
{
    // 00800614 <sys_pgdir>
    // 正常运行，打印页表
    asm volatile("call $0x18, $0x00800614;");
    cprintf("call sys_pgdir success.\n");
}

void test_jmp_user_func(void)
{
    // 00800614 <sys_pgdir>
    // error: not valid addr 1b, and  can not find it in vma
    // 跳过去之后回不来了
    asm volatile("ljmp $0x18, $0x00800614;");
    cprintf("ljmp sys_pgdir success.\n");
}

void test_call_kernel_func(void)
{
    // 0x8 = 00001 0 00 内核代码段选择子
    // 0xc010090d <print_kerninfo>
    // 触发 General Protection，内核代码并不能直接跳转访问，是因为内核为了安全，
    // 代码都是非一致性代码，DPL 也是 0 环
    asm volatile("call $0x8, $0xc010090d;");// ljmp $0x8, $0xc010090d
    cprintf("call to kernel.\n");
}

void test_jmp_kernel_func(void)
{
    // 0x8 = 00001 0 00 内核代码段选择子
    // 0xc010090d <print_kerninfo>
    // 触发 General Protection，内核代码并不能直接跳转访问，是因为内核为了安全，
    // 代码都是非一致性代码，DPL 也是 0 环
    asm volatile("ljmp $0x8, $0xc010090d;");// ljmp $0x8, $0xc010090d
    cprintf("jump to kernel.\n");
}

void test_read_illegal_addr_func(void)
{
    // 访问地址在当前进程页表里找不到映射，会产生 T_PGFLT page fault 中断
    // 操作系统检测进程 mm 表，发现当前地址也不在该进程可访问的空间内，直接 kill 进程
    
    // error: not valid addr 0, and  can not find it in vma
    cprintf("I read %8x from 0.\n", *(unsigned int *)0);
    
    // error: not valid addr c010090d, and  can not find it in vma
    cprintf("I read %8x from 0xc010090d.\n", *(unsigned int *)0xc010090d);
}

void test_task_gate(void)
{
    // 通过任务门来访问内核函数
    asm volatile ("int $0xff");
    cprintf("task gate.\n");
}

void test_call_gate(void)
{
    // 通过系统提供的调用门的方式来访问内核函数
    // 0x30 = 00110 0 00 调用门段选择子
    asm volatile("mov $0x77, %ebx;");
    asm volatile("mov $0x88, %ecx;");
    asm volatile("mov $0x99, %edx;");
    asm volatile("call $0x33, $0;");
    cprintf("call gate.\n");
}

int main(void)
{
//    test_call_user_func();
//    test_call_kernel_func();
//    test_read_illegal_addr_func();
//    test_task_gate();
    test_call_gate();
}


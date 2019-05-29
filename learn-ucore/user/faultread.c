#include "stdio.h"
#include "ulib.h"
#include "unistd.h"

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
    asm volatile ("int $0x90");
    cprintf("task gate.\n");
}

int test_call_gate(int num, ...)
{
#define MAX_ARGS            5
    va_list ap;
    va_start(ap, num);
    uint32_t a[MAX_ARGS];
    int i, ret;
    for (i = 0; i < MAX_ARGS; i ++)
    {
        a[i] = va_arg(ap, uint32_t);
    }
    va_end(ap);

    asm volatile (
                  "call $0x30, $0;"
                  : "=a" (ret)
                  : "a" (num),
                  "d" (a[0]),
                  "c" (a[1]),
                  "b" (a[2]),
                  "D" (a[3]),
                  "S" (a[4])
                  : "cc", "memory");
    cprintf("call gate.\n");
    return ret;
}

int main(void)
{
//    test_call_user_func();
//    test_call_kernel_func();
//    test_read_illegal_addr_func();
    test_call_gate(SYS_pgdir);
    test_task_gate();
}


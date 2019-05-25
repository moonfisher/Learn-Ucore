#include "stdio.h"
#include "ulib.h"

int main(void)
{
    // 0x30 = 00110 0 00 调用门段选择子，通过调用门提权
    asm volatile("movw $0x30, %ax;");
    asm volatile("movw %ax, %ds");
    cprintf("set ds to user data.\n");
    
    // 访问地址在当前进程页表里找不到映射，会产生 T_PGFLT page fault 中断
    // 操作系统检测进程 mm 表，发现当前地址也不在该进程可访问的空间内，直接 kill 进程
    cprintf("I read %08x from 0xfac00000!\n", *(unsigned *)0xfac00000);
    panic("FAIL: T.T\n");
}


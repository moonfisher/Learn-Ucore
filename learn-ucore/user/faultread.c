#include "stdio.h"
#include "ulib.h"

int main(void)
{
    // 访问地址在当前进程页表里找不到映射，会产生 T_PGFLT page fault 中断
    // 操作系统检测进程 mm 表，发现当前地址也不在该进程可访问的空间内，直接 kill 进程
    cprintf("I read %8x from 0.\n", *(unsigned int *)0);
    panic("FAIL: T.T\n");
}


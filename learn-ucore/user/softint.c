#include "stdio.h"
#include "ulib.h"

int main(void)
{
    // 会触发 T_GPFLT general protection fault 中断
    asm volatile("int $14");
    panic("FAIL: T.T\n");
}


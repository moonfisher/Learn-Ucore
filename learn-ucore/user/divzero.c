#include "stdio.h"
#include "ulib.h"

int zero = 0;

int main(void)
{
    // 会触发 T_DIVIDE divide error 中断
    cprintf("value is %d.\n", 1 / zero);
    panic("FAIL: T.T\n");
}


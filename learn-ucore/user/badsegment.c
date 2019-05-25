#include "stdio.h"
#include "ulib.h"

/* try to load the kernel's TSS selector into the DS register */

int main(void)
{
    // 0x20 = 00100 0 00 User Data 段选择子
    asm volatile("movw $0x20, %ax;");
    asm volatile("movw %ax, %ds");
    cprintf("set ds to user data.\n");
    
    // 0x18 = 00011 0 00 User Code 段选择子
    asm volatile("movw $0x18, %ax;");
    asm volatile("movw %ax, %ds");
    cprintf("set ds to user code.\n");
    
    // 0x28 = 00101 0 00 TSS 段选择子
    // 当前运行在用户态，cpl = 3，tss 的 dpl = 0，会触发 T_GPFLT general protection fault 中断
//    asm volatile("movw $0x28, %ax; movw %ax, %ds");
    asm volatile("movw $0x28, %ax;");
    asm volatile("movw %ax, %ds");
    panic("FAIL: T.T\n");
}


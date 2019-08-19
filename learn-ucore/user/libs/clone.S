#include "unistd.h"

.text
.globl __clone
__clone:                        # __clone(clone_flags, stack, fn, arg)
    pushl %ebp                  # maintain ebp chain
    movl %esp, %ebp

    pushl %edx                  # save old registers
    pushl %ecx
    pushl %ebx
    pushl %edi

    movl 0x8(%ebp), %edx        # load clone_flags
    movl 0xc(%ebp), %ecx        # load stack
    movl 0x10(%ebp), %ebx       # load fn
    movl 0x14(%ebp), %edi       # load arg

    # sys_clone 函数里，内核只用到了 clone_flags 和 stack 两个参数
    # fn 和 arg 属于用户层参数，内核无需关心
    movl $SYS_clone, %eax       # load SYS_clone
    int $T_SYSCALL              # syscall

    # clone 完成之后，父进程和子进程都会从这里返回
    cmpl $0x0, %eax             # pid ? child or parent ?
    je 1f                       # eax == 0, goto 1;

    # parent
    popl %edi                   # restore registers
    popl %ebx
    popl %ecx
    popl %edx

    leave                       # restore ebp
    ret

    # child
1:
    # 从这里可以看到，子进程入口函数地址，以及父进程给子进程传递的参数
    # 都是在用户层控制的，内核并不关心入口函数地址和参数，内核只是创建了一个新的 task
    pushl %edi
    call *%ebx                  # call fn(arg)
    # 子进程退出之后的返回值也是通过 eax 返回
    movl %eax, %edx             # save exit_code
    movl $SYS_exit_thread, %eax # load SYS_exit_thread
    int $T_SYSCALL              # int SYS_exit_thread

spin:                           # error ?
    jmp spin


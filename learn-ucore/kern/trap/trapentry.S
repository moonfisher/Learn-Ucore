#include "memlayout.h"

# vectors.S sends all traps here.
.text
.globl __alltraps
__alltraps:
    # push registers to build a trap frame
    # therefore make the stack look like a struct trapframe
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    pushal

    # load GD_KDATA into %ds and %es to set up data segments for kernel
    # 中断服务都是内核提供，这里把数据段设置为内核段
    movl $GD_KDATA, %eax
    movw %ax, %ds
    movw %ax, %es

    # push %esp to pass a pointer to the trapframe as an argument to trap()
    # 这里把堆栈当前地址也作为参数压栈，可以在 trap() 中修改 esp 地址，从而实现修改栈帧
    pushl %esp

    # call trap(tf), where tf=%esp
    call trap

    # pop the pushed stack pointer
    # 如果在 trap() 中修改了 esp 地址，此时返回之后，esp 将指向新的栈帧，后续 pop 的参数也不同
    # 内核模式和用户模式的切换，就是通过中断，然后修改栈帧让 cpu 执行不同代码段实现
    popl %esp

    # return falls through to trapret...
.globl __trapret
__trapret:
    # restore registers from stack
    popal

    # restore %ds, %es, %fs and %gs
    popl %gs
    popl %fs
    popl %es
    popl %ds

    # get rid of the trap number and error code
    # 这里 add 0x8 是跳过之前压栈的中断码和错误码
    addl $0x8, %esp

    # ret：也可以叫做近返回，即段内返回。处理器从堆栈中弹出IP或者EIP，
    # 然后根据当前的 CS：IP 跳转到新的执行地址。如果之前压栈的还有其余的参数，则这些参数也会被弹出。

    # iret：用于从中断返回，会弹出 EIP，CS，以及一些标志。然后从 CS：EIP 执行。
    # 所以无论是进程切换，还是内核态切换到用户态，通过中断，构造返回的中断桢，能达到修改 CS 的目的
    iret

.globl forkrets # 创建进程或者切换进程之后的入口
forkrets:
    # set stack to this new process's trapframe
    movl 4(%esp), %esp
    jmp __trapret


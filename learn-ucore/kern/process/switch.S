.text
.globl switch_to
switch_to:                      # switch_to(from, to)

    # save from's registers
    # 保存前一个进程的执行现场，前两条汇编指令保存了进程在返回 switch_to 函数后的指令地址到 context.eip 中：
    movl 4(%esp), %eax          # eax points to from
    popl 0(%eax)                # save eip !popl

    # 然后接下来的七条汇编指令完成了保存前一个进程的其他 7 个寄存器到 context 中的相应成员变量中。
    # 至此前一个进程的执行现场保存完毕。
    # 保存堆栈当前位置
    movl %esp, 4(%eax)
    movl %ebx, 8(%eax)
    movl %ecx, 12(%eax)
    movl %edx, 16(%eax)
    movl %esi, 20(%eax)
    movl %edi, 24(%eax)
    movl %ebp, 28(%eax)

    # restore to's registers
    # 恢复向一个进程的执行现场，这其实就是上述保存过程的逆执行过程，
    # 即从 context 的高地址的成员变量 ebp 开始，逐一把相关域的值赋值给对应的寄存器。
    movl 4(%esp), %eax          # not 8(%esp): popped return address already
                                # eax now points to to
    movl 28(%eax), %ebp
    movl 24(%eax), %edi
    movl 20(%eax), %esi
    movl 16(%eax), %edx
    movl 12(%eax), %ecx
    movl 8(%eax), %ebx
    # 堆栈恢复到上一次执行的地方
    movl 4(%eax), %esp

    # pushl 0(%eax)其实把 context 中保存的下一个进程要执行的指令地址 context.eip 放到了堆栈顶，
    # 这样接下来执行最后一条指令 ret 时, 会把栈顶的内容赋值给 EIP 寄存器，这样就切换到下一个进程执行了，
    # 即当前进程已经是下一个进程了，从而完成了进程的切换。
    pushl 0(%eax)               # push eip

    # 如果是 task 首次加载运行，这里 ret 之后，会返回到切换后的进程执行入口 forkret
    # 如果是 task 调度运行，这里 ret 之后，返回到上次这个进程的执行现场
    ret


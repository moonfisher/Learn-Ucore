.text
.globl kernel_thread_entry
kernel_thread_entry:        # void kernel_thread(void)

    pushl %edx              # push arg
    call *%ebx              # call fn 这里就是每个进程的 main 函数入口

    # 进程在 main 函数里自己 return 之后，会往下走
    pushl %eax              # save the return value of fn(arg)
    call do_exit            # call do_exit to terminate current thread


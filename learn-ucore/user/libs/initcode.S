# 这里 _start 才是用户程序执行真正的函数入口地址，在用户程序编译链接时由 user.ld 文件决定
# 常见的 main 函数并不是真正的入口
.text
.globl _start
_start:
    # set ebp for backtrace
    movl $0x0, %ebp

    # load argc and argv
    movl (%esp), %ebx
    lea 0x4(%esp), %ecx


    # move down the esp register
    # since it may cause page fault in backtrace
    subl $0x20, %esp

    # save argc and argv on stack
    # ecx 存放的 argc，ebx 地址指向 argv
    pushl %ecx
    pushl %ebx

    # call user-program function
    call umain
1:  jmp 1b

    

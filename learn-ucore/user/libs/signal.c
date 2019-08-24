#include "syscall.h"
#include "signal.h"
#include "string.h"
#include "stdio.h"
#include "x86.h"

// 用户层在 signal handle 处理完之后，会来到这里
// 这里需要找到进入 hanlde 之前的堆栈地址，那里记录了处理 signal 之前的旧的 tf 中断桢结构
// 通过旧的 tf 中断桢，重新进入内核态，再次返回时，用户进程可以返回到最开始进入内核态时的地址，
// 这样 signal 是异步处理的，用户代码上没有连续性，也感觉不到 signal 的调用时机
void sig_restorer(int sign)
{
    uint32_t oldesp = read_ebp();
    cprintf("signal restorer by %d\n", sys_getpid());
    sys_sigreturn(oldesp);
}

int signal(int sign, sighandler_t handler)
{
    struct sigaction act = { handler, NULL, 1 << (sign - 1), 0, sig_restorer };
    return sys_sigaction(sign, &act, NULL);
}

int sigaction(int sign, struct sigaction *act)
{
    return sys_sigaction(sign, act, NULL);
}

int tkill(int pid, int sign)
{
	return sys_sigtkill(pid, sign);
}

int sigprocmask(int how, const sigset_t *set, sigset_t *old)
{
	return sys_sigprocmask(how, set, old);
}

int sigsuspend(uint32_t mask)
{
	return sys_sigsuspend(mask);
}

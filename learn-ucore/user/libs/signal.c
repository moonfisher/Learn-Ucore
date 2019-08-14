#include "syscall.h"
#include "signal.h"

void sig_restorer(int sign)
{
    sys_sigreturn(0);
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

int sigreturn()
{
    return sys_sigreturn(0);
}

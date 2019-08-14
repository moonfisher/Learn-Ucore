#include "syscall.h"
#include "signal.h"

int signal(int sign, sighandler_t handler)
{
    struct sigaction act = { handler, NULL, 1 << (sign - 1), 0 };
    return sys_sigaction(sign, &act, NULL);
}

int tkill(int pid, int sign)
{
	return sys_tkill(pid, sign);
}

int sigprocmask(int how, const sigset_t * set, sigset_t * old)
{
	return sys_sigprocmask(how, set, old);
}

int sigsuspend(uint32_t mask)
{
	return sys_sigsuspend(mask);
}

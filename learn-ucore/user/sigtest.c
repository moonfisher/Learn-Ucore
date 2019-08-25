#include "ulib.h"
#include "stdio.h"
#include "signal.h"

#define child_num       1

void handler(int sign)
{
	cprintf("signal handler: %d received by %d\n", sign, getpid());
}

int child()
{
	int pid = getpid();
	cprintf("IM child %d\n", pid);

    signal(SIGUSR1, handler);
    signal(SIGUSR2, handler);

    struct sigaction act = { handler, NULL, 1 << (SIGALRM - 1), 0, NULL };
    sigaction(SIGALRM, &act);
    
	sigset_t set = 1ull << (SIGUSR1 - 1);
	cprintf("%d block SIGUSER1\n", pid);
	sigprocmask(SIG_BLOCK, &set, NULL);

	int l = 0;
	while (1)
    {
		if (++l == 9)
        {
			cprintf("%d unblock SIGUSER1\n", pid);
			sigprocmask(SIG_UNBLOCK, &set, NULL);
		}
		cprintf("im %d\n", pid);
		sleep(100);
	}
}

int main()
{
    int children[child_num] = {0};
	int i;
	for (i = 0; i < child_num; ++i)
    {
		sleep(20);
		int pid = fork("child");
		if (pid == 0)
        {
			child();
        }
		else
        {
			children[i] = pid;
        }
	}
    
	sleep(100);
	for (i = 0; i < child_num; ++i)
    {
		sleep(20);
		cprintf("send SIGUSR1 to %d\n", children[i]);
		tkill(children[i], SIGUSR1);
		cprintf("SIGUSR1 end\n");
	}
    
	sleep(100);
	for (i = 0; i < child_num; ++i)
    {
		sleep(20);
		cprintf("send SIGUSR2 to %d\n", children[i]);
		tkill(children[i], SIGUSR2);
		cprintf("SIGUSR2 end\n");
	}
    
    sleep(100);
    for (i = 0; i < child_num; ++i)
    {
        sleep(20);
        cprintf("send SIGUSR2 to %d\n", children[i]);
        tkill(children[i], SIGALRM);
        cprintf("SIGUSR2 end\n");
    }
    
	sleep(400);
	for (i = 0; i < child_num; ++i)
    {
		sleep(20);
		cprintf("send SIGKILL to %d\n", children[i]);
		tkill(children[i], SIGKILL);
		cprintf("SIGKILL end\n");
	}
    
	return 0;
}

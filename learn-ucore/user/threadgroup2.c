#include "ulib.h"
#include "stdio.h"
#include "thread.h"
#include "string.h"

#define     threadnum       3

int thread_loop(void *arg)
{
	char *name = (char *)arg;
    int ret = strcmp(name, "0");
    while (ret)
    {
        sleep(100);
    }

	int i = 0;
	for (i = 0; i < 3; i++)
    {
		cprintf("yield %d.\n", i);
		yield();
	}
	cprintf("exit thread group now.\n");
	exit(0);
}

int main(void)
{
	thread_t tids[threadnum];
    char local_name[threadnum][20];

	int i = 0;
	for (i = 0; i < threadnum; i++)
    {
        memset(local_name[i], 0, 20);
        snprintf(local_name[i], 20, "%d", i);
		if (thread(thread_loop, local_name[i], tids + i) != 0)
        {
			goto failed;
		}
	}

	cprintf("thread ok.\n");

    while (1)
    {
        sleep(100);
    }

	return 0;

failed:
	for (i = 0; i < threadnum; i++)
    {
		if (tids[i].pid > 0)
        {
			kill(tids[i].pid);
		}
	}
	panic("FAIL: T.T\n");
}

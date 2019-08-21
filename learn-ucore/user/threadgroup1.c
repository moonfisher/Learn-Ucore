#include "ulib.h"
#include "stdio.h"
#include "thread.h"
#include "string.h"

#define     threadnum       20

int thread_loop(void *arg)
{
    while (1)
    {
        ;
    }
}

int main(void)
{
	thread_t tids[threadnum];

	int i = 0;
    char local_name[threadnum][20];
	for (i = 0; i < threadnum; i++)
    {
        memset(local_name[i], 0, 20);
        snprintf(local_name[i], 20, "thread-%d", i);
		if (thread(thread_loop, local_name[i], tids + i) != 0)
        {
			goto failed;
		}
	}

	cprintf("thread ok.\n");

	for (i = 0; i < 3; i++)
    {
		cprintf("yield %d.\n", i);
        // yield 等于主动让出 cpu，触发内核 schedule
		yield();
	}
    
    // 这里是主线程退出了，等同于进程退出了，所有子线程都会退出
	cprintf("exit thread group now.\n");
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

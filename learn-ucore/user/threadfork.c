#include "ulib.h"
#include "stdio.h"
#include "thread.h"
#include "string.h"

#define     forknum         8
#define     threadnum       6

void process_main(void)
{
	sleep(10);
}

int thread_main(void *arg)
{
	int i, pid;
    char *thread_name = (char *)arg;
    char local_name[30];
    char tmp_name[20];
	for (i = 0; i < forknum; i++)
    {
        memset(local_name, 0, sizeof(local_name));
        strncpy(local_name, thread_name, strlen(thread_name));
        
        memset(tmp_name, 0, sizeof(tmp_name));
        snprintf(tmp_name, sizeof(tmp_name), "-fork-%d", i);
        
        strcat(local_name, tmp_name);
        
		if ((pid = fork(local_name)) == 0)
        {
			process_main();
			exit(0);
		}
	}
	sleep(200);
	return 0;
}

int main(void)
{
	thread_t tids[threadnum];

    int i = 0;
    char local_name[20];
	for (i = 0; i < threadnum; i++)
    {
        memset(local_name, 0, sizeof(local_name));
        snprintf(local_name, sizeof(local_name), "thread-%d", i);
		if (thread(thread_main, local_name, tids + i) != 0)
        {
			goto failed;
		}
	}

	int count = 0;
	while (wait() == 0)
    {
		count++;
	}

    assert(count == (forknum + 1) * threadnum);
	cprintf("threadfork pass, count = %d.\n", count);
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

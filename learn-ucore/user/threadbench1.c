#include "ulib.h"
#include "stdio.h"
#include "thread.h"
#include "string.h"

#define printf(...)                     fprintf(1, __VA_ARGS__)

#define NR_LOOPS    10000
#define NR_THREADS  10

int test(void *arg)
{
	int pid = getpid();
	printf("Child thread pid = %d\n", pid);

	int i, j;
	for (i = 0; i < NR_LOOPS; i++)
		for (j = 0; j < NR_LOOPS; j++) ;

	return 0xbee;
}

int main(void)
{
	int pid = getpid();
	printf("The main thread pid = %d\n", pid);

	thread_t tid[NR_THREADS];
	int i;

    char local_name[NR_THREADS][20];
	for (i = 0; i < NR_THREADS; i++)
    {
        memset(local_name[i], 0, 20);
        snprintf(local_name[i], 20, "thread-%d", i);
		if (thread(test, local_name[i], tid + i) != 0)
			printf("Thread %d is not created.\n", i);
	}

	int exit_code;
	for (i = 0; i < NR_THREADS; i++)
    {
		if (thread_wait(tid + i, &exit_code) != 0 || exit_code != 0xbee)
			printf("Thread %d is not run normally.\n", i);
	}

	cprintf("thread testbench1 exits.\n");
	return 0;
}

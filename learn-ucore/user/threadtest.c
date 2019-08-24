#include "ulib.h"
#include "stdio.h"
#include "thread.h"

int test(void *arg)
{
    char *name = (char *)arg;
	cprintf("test %s ok.\n", name);
    print_pgdir();
    print_vm();
	return 0xbee;
}

int main(void)
{
	thread_t tid;
	assert(thread(test, "child", &tid) == 0);
	cprintf("thread ok, tid = %d.\n", tid);

	int exit_code;
	assert(thread_wait(&tid, &exit_code) == 0 && exit_code == 0xbee);

	cprintf("threadtest pass.\n");
	return 0;
}

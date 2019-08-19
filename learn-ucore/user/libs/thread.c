#include "defs.h"
#include "ulib.h"
#include "thread.h"
#include "unistd.h"
#include "error.h"

int thread(int (*fn)(void *), void *arg, thread_t *tidp)
{
	if (fn == NULL || tidp == NULL)
    {
		return -E_INVAL;
	}
    
    // 从进程栈空间地址往下（mmap 区域），寻找可用的虚拟地址空间，作为线程的栈空间
    // 这里只是分配了虚拟地址空间，线程在开始运行使用堆栈时，会发生缺页中断，从而申请内存
	int ret;
	uintptr_t stack = 0;
	if ((ret = mmap(&stack, THREAD_STACKSIZE, MMAP_WRITE | MMAP_STACK)) != 0)
    {
		return ret;
	}
	assert(stack != 0);
    print_pgdir();

	if ((ret = clone(CLONE_VM | CLONE_THREAD | CLONE_FS, stack + THREAD_STACKSIZE, fn, arg)) < 0)
    {
		munmap(stack, THREAD_STACKSIZE);
		return ret;
	}

	tidp->pid = ret;
	tidp->stack = (void *)stack;
	return 0;
}

int thread_wait(thread_t * tidp, int *exit_code)
{
	int ret = -E_INVAL;
	if (tidp != NULL)
    {
		if ((ret = waitpid(tidp->pid, exit_code)) == 0)
        {
			munmap((uintptr_t) (tidp->stack), THREAD_STACKSIZE);
		}
	}
	return ret;
}

int thread_kill(thread_t * tidp)
{
	if (tidp != NULL)
    {
		return kill(tidp->pid);
	}
	return -E_INVAL;
}

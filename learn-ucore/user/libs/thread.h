#ifndef __USER_LIBS_THREAD_H__
#define __USER_LIBS_THREAD_H__

#define     THREAD_STACKSIZE        (4096 * 10)

typedef struct
{
	int pid;
	void *stack;
} thread_t;

int thread(int (*fn)(void *), void *arg, thread_t *tidp);
int thread_wait(thread_t *tidp, int *exit_code);
int thread_kill(thread_t *tidp);

#endif /* !__USER_LIBS_THREAD_H__ */

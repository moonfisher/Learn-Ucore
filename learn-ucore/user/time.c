#include "ulib.h"
#include "stdio.h"
#include "syscall.h"

#define printf(...)                     fprintf(1, __VA_ARGS__)

int main(int argc, const char **argv)
{
    cprintf("time main start.\n");
    
	int pid;
	argv[argc] = 0;
	uint64_t before = sys_gettime();

	if (argc > 1)
    {
		pid = fork((char *)argv[1]);
		if (pid < 0)
        {
			printf("Error: cannot fork process.\n");
		}
        else if (pid == 0)
        {
			__exec(argv[1], argv + 1);
		}
		waitpid(pid, NULL);
	}

	uint64_t after = sys_gettime();

	printf("\n ===== Summary ===== \n");
	printf("exec %ld ticks.\n", after - before);
	printf("\n");
	return 0;
}

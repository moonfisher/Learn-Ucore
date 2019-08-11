#include "ulib.h"
#include "stdio.h"
#include "unistd.h"
#include "malloc.h"
#include "string.h"

void *buf1, *buf2;

int main(void)
{
	assert((buf1 = shmem_malloc(256)) != NULL);
	assert((buf2 = malloc(256)) != NULL);
	
	int i;
	for (i = 0; i < 256; ++i)
    {
		//cprintf("i= %x\n",i);
		*(char *)(buf1 + i) = (char)i;
	}
	memset(buf2, 0, 256);
    *((char *)buf2 + 2) = 'c';
    
    int pid, exit_code;

    if ((pid = fork("child")) == 0)
    {
    	cprintf("child pid = %x\n", getpid());
    	for (i = 0; i < 256; i++)
        {
            assert(*(char *)(buf1 + i) == (char)i);
        }
        memset(buf1, 0x88, 256);
        memset(buf2, 0x77, 256);
        exit(0);
    }
    else
    {
    	assert(pid > 0 && waitpid(pid, &exit_code) == 0 && exit_code == 0);
	    free(buf1);
	    free(buf2);
	    cprintf("shmemtest pass.\n");
    }
    
	return 0;
}

















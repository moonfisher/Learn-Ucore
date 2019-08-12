#include "ulib.h"
#include "stdio.h"
#include "unistd.h"
#include "malloc.h"
#include "string.h"

#define malloc_size         2048

int main(void)
{
    uint8_t *buf1;
    uint8_t *buf2;
    
    assert((buf1 = shmem_malloc(malloc_size)) != NULL);
	assert((buf2 = malloc(malloc_size)) != NULL);
	
	int i = 0;
	for (i = 0; i < malloc_size; ++i)
    {
        *(uint8_t *)(buf1 + i) = 0x33;
        *(uint8_t *)(buf2 + i) = 0x44;
	}

    int pid = 0;
    int exit_code = 0;
    int a = 0x88;

    if ((pid = fork("child")) == 0)
    {
        print_pgdir();
        
    	cprintf("child pid = %x, a = 0x%x.\n", getpid(), a);
        a = 0x99;
        cprintf("child pid = %x, a = 0x%x.\n", getpid(), a);
        
        // fork 出来的子进程是共享父进程页表的，变量虚拟地址都是一样的，可以正常读取父进程变量
    	for (i = 0; i < malloc_size; i++)
        {
            assert(*(uint8_t *)(buf1 + i) == 0x33);
            assert(*(uint8_t *)(buf2 + i) == 0x44);
        }
        
        // buf1 是共享内存，子进程可以修改
        memset(buf1, 0x88, malloc_size);
        // buf2 是父进程自己的内存，子进程修改的话会触发缺页中断，申请自己的物理内存去修改
        memset(buf2, 0x77, malloc_size);
        
        exit(0);
    }
    else
    {
    	assert(pid > 0 && waitpid(pid, &exit_code) == 0 && exit_code == 0);
        
        for (i = 0; i < malloc_size; i++)
        {
            assert(*(uint8_t *)(buf1 + i) == 0x88);
        }
	    free(buf1);
        
        for (i = 0; i < malloc_size; i++)
        {
            assert(*(uint8_t *)(buf2 + i) == 0x44);
        }
	    free(buf2);
        
	    cprintf("parent pid = %x, shmemtest pass, a = 0x%x\n", getpid(), a);
    }
    
	return 0;
}

















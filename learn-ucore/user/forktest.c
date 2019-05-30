#include "ulib.h"
#include "stdio.h"
#include "string.h"

const int max_child = 128;

int main(void)
{
    int n, pid;
    for (n = 0; n < max_child; n ++)
    {
        char local_name[20];
        memset(local_name, 0, sizeof(local_name));
        snprintf(local_name, sizeof(local_name), "pid-%d", n);
        
        if ((pid = fork(local_name)) == 0)
        {
            cprintf("I am child %d\n", n);
            int m = 10000;
            while (m < 0) {
                m--;
            }
            exit(0);
        }
        assert(pid > 0);
    }

    if (n > max_child)
    {
        panic("fork claimed to work %d times!\n", n);
    }

    for (; n > 0; n --)
    {
        if (wait() != 0)
        {
            panic("wait stopped early\n");
        }
    }

    if (wait() == 0)
    {
        panic("wait got too many\n");
    }

    cprintf("forktest pass.\n");
    return 0;
}


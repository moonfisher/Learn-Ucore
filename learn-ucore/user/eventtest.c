#include "stdio.h"
#include "ulib.h"
#include "error.h"

#define event_type      0x10

void test1(void)
{
    int pid, parent = getpid();
    if ((pid = fork("child1")) == 0)
    {
        cprintf("child1 process\n");
        //子进程
        int event, sum = 0, ret = 0;
        while ((ret = recv_event(&pid, event_type, &event)) == 0 && parent == pid)
        {
            if (event == -1)
            {
                cprintf("child1 Hmmm!\n");
                sleep(100);
                cprintf("child1 quit\n");
                exit(0);
            }
            cprintf("child1 receive %d from %d\n", event, pid);
            sum += event;
        }
        panic("FAIL: T.T\n");
    }
    else
    {
        cprintf("parent process\n");
    }
    //父、子进程
    assert(pid > 0);
    int i = 10;
    while (send_event(pid, event_type, i) == 0)
    {
        cprintf("test1 parent send i = %d\n", i);
        i--;
        sleep(50);
    }
    cprintf("test1 pass.\n");
}

void test3(void)
{
    int pid;
    if ((pid = fork("child3")) == 0)
    {
        int event;
        if (recv_event_timeout(NULL, event_type, &event, 100) == -E_TIMEOUT)
        {
            cprintf("test3 pass.\n");
        }
        exit(0);
    }
    assert(pid > 0);
    assert(waitpid(pid, NULL) == 0);
}

int main(void)
{
    test1();
    test3();
    cprintf("eventtest pass.\n");
    return 0;
}


#include "stdio.h"
#include "ulib.h"
#include "dir.h"

int main(void)
{
    char buffer[1024] = {0};
    getcwd(buffer, 1024);
    cprintf("Hello world!!. path = %s.\n", buffer);
    cprintf("I am process %d.\n", getpid());
    cprintf("hello pass.\n");
    
    return 0;
}


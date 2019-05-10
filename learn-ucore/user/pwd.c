#include <ulib.h>
#include <stdio.h>
#include <dir.h>

#define printf(...)                     fprintf(1, __VA_ARGS__)
#define BUFSIZE                         4096

// 这里 pwd 执行 getcwd 虽然获取是的当前 pwd 进程的目录，其实也是进程 sh 的目录
// 因为 pwd 的父进程是 sh，进程 fork 的时候 pwd 复制了 sh 进程当前的目录
int main(int argc, char **argv)
{
    int ret;
    static char cwdbuf[BUFSIZE];
    if ((ret = getcwd(cwdbuf, sizeof(cwdbuf))) != 0)
    {
        return ret;
    }
    printf("current dir is [%s].\n", cwdbuf);
	return 0;
}

#include "ulib.h"
#include "file.h"
#include "stat.h"
#include "unistd.h"
#include "dir.h"

// cd xxx，需要切换的是父进程 sh 所在的文件目录，这里不能用子进程 cd 去切换
// 子进程 cd 切换的只是自己当前的目录结构
int main(int argc, char **argv)
{
	if (argc == 2)
    {
        int ret = chdir(argv[1]);
        return ret;
	}
    
	return 0;
}

#include "ulib.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stat.h"
#include "../libs/file.h"
#include "dir.h"
#include "unistd.h"

#define printf(...)                 fprintf(1, __VA_ARGS__)

static int safe_open(const char *path, int open_flags)
{
	int fd = open(path, open_flags);
	printf("fd is %d\n", fd);
	assert(fd >= 0);
	return fd;
}

static struct stat *safe_fstat(int fd)
{
	static struct stat __stat, *stat = &__stat;
	int ret = fstat(fd, stat);
	assert(ret == 0);
	return stat;
}

static int safe_read(int fd, void *data, size_t len)
{
    int ret = read(fd, data, len);
    return ret;
}

static int safe_write(int fd, void *data, size_t len)
{
    int ret = write(fd, data, len);
    return ret;
}

static int safe_close(int fd)
{
    int ret = close(fd);
    return ret;
}

/*
 通用文件访问接口层的处理流程
 首先进入通用文件访问接口层的处理流程，即进一步调用如下用户态函数： open->sys_open->syscall，
 从而引起系统调用进入到内核态。到了内核态后，通过中断处理例程，会调用到 sys_open 内核函数，并进一步
 调用 sysfile_open 内核函数。到了这里，需要把位于用户空间的字符串 "/test/testfile" 拷贝到内核空间
 中的字符串 path 中，并进入到文件系统抽象层的处理流程完成进一步的打开文件操作中。
 
 文件接口抽象层的处理流程
 分配一个空闲的 file 数据结构变量 file 在文件系统抽象层的处理中，首先调用的是 file_open 函数，
 它要给这个即将打开的文件分配一个 file 数据结构的变量，这个变量其实是当前进程的打开文件数组
 current->fs_struct->filemap[] 中的一个空闲元素（即还没用于一个打开的文件），而这个元素的索引值
 就是最终要返回到用户进程并赋值给变量 fd1。到了这一步还仅仅是给当前用户进程分配了一个 file 数据结构
 的变量，还没有找到对应的文件索引节点。为此需要进一步调用 vfs_open 函数来找到 path 指出的文件所对应
 的基于 inode 数据结构的 VFS 索引节点 node。vfs_open 函数需要完成两件事情：通过 vfs_lookup 找到
 path 对应文件的 inode；调用 vop_open 函数打开文件。
 
 找到文件设备的根目录 “/” 的索引节点需要注意，这里的 vfs_lookup 函数是一个针对目录的操作函数，它会
 调用 vop_lookup 函数来找到 SFS 文件系统中的 “/test” 目录下的 “testfile” 文件。为此，vfs_lookup
 函数首先调用 get_device 函数，并进一步调用 vfs_get_bootfs 函数（其实调用了）来找到根目录 “/” 对应
 的 inode。这个 inode 就是位于 vfs.c 中的 inode 变量 bootfs_node。这个变量在 init_main 函数
 （位于kern/process/proc.c）执行时获得了赋值。
 
 找到根目录 “/” 下的 “test” 子目录对应的索引节点，在找到根目录对应的 inode 后，通过调用 vop_lookup
 函数来查找 “/” 和 “test” 这两层目录下的文件 “testfile” 所对应的索引节点，如果找到就返回此索引节点。
 
 把 file 和 node 建立联系。完成第3步后，将返回到 file_open 函数中，通过执行语句 “file->node=node;”，
 就把当前进程的 current->fs_struct->filemap[fd]（即file所指变量）的成员变量 node 指针指向了代
 表 “/test/testfile” 文件的索引节点 node。这时返回 fd。经过重重回退，通过系统调用返回，用户态的
 syscall->sys_open->open->safe_open 等用户函数的层层函数返回，最终把把 fd 赋值给 fd1。自此完成了
 打开文件操作。但这里我们还没有分析第 2 和第 3 步是如何进一步调用 SFS 文件系统提供的函数找位于 SFS 文件
 系统上的 “/test/testfile” 所对应的 sfs 磁盘 inode 的过程。
 
 SFS文件系统层的处理流程
 这里需要分析文件系统抽象层中没有彻底分析的 vop_lookup 函数到底做了啥。在 sfs_inode.c 中的
 sfs_node_dirops 变量定义了 “.vop_lookup = sfs_lookup”，所以我们重点分析 sfs_lookup 的实现。
 sfs_lookup 有三个参数：node，path，node_store。其中node是根目录 “/” 所对应的 inode 节点；
 path 是文件 “testfile” 的绝对路径 “/test/testfile”，而 node_store 是经过查找获得的 “testfile”
 所对应的 inode 节点。
 
 sfs_lookup 函数以 “/” 为分割符，从左至右逐一分解 path 获得各个子目录和最终文件对应的 inode 节点。
 在本例中是分解出 “test” 子目录，并调用 sfs_lookup_once 函数获得 “test” 子目录对应的 inode 节点
 subnode，然后循环进一步调用 sfs_lookup_once 查找以 “test” 子目录下的文件 “testfile1” 所对应的
 inode 节点。当无法分解 path 后，就意味着找到了 testfile1 对应的 inode 节点，就可顺利返回了。
 
 当然这里讲得还比较简单，sfs_lookup_once 将调用 sfs_dirent_search_nolock函数来查找与路径名匹配的目录项，
 如果找到目录项，则根据目录项中记录的 inode 所处的数据块索引值找到路径名对应的 SFS 磁盘 inode，并读
 入 SFS 磁盘 inode 对的内容，创建 SFS 内存 inode。
*/
int main(void)
{
//    char buffer1[1024] = {0};
//
//    int fd1 = safe_open("sfs_filetest1", O_RDONLY);
//    struct stat *stat1 = safe_fstat(fd1);
//    assert(stat1->st_size >= 0 && stat1->st_blocks >= 0);
//
//    int ret1 = safe_read(fd1, buffer1, sizeof(buffer1) - 1);
//    printf("sfs_filetest1 length = %d, read %s.\n", ret1, buffer1);
//    ret1 = safe_close(fd1);
//
//    printf("sfs_filetest1 pass.\n");
    
    char buffer2[1024] = {0};
    
    int fd2 = safe_open("test222", O_RDWR | O_TRUNC | O_CREAT | O_APPEND);
    struct stat *stat2 = safe_fstat(fd2);
    assert(stat2->st_size >= 0 && stat2->st_blocks >= 0);
    
    char str[] = "Hello, world\r\n";
    int ret2 = safe_write(fd2, str, sizeof(str) - 1);
    ret2 = safe_close(fd2);
    
    fd2 = safe_open("test222", O_RDWR | O_TRUNC | O_CREAT);
    ret2 = safe_read(fd2, buffer2, sizeof(buffer2) - 1);
    printf("test222 length = %d, read %s.\n", ret2, buffer2);
    ret2 = safe_close(fd2);

	return 0;
}

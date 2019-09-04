#ifndef __KERN_FS_FILE_H__
#define __KERN_FS_FILE_H__

#include "defs.h"
#include "fs.h"
#include "proc.h"
#include "atomic.h"
#include "assert.h"

/*
 文件系统抽象层是把不同文件系统的对外共性借口提取出来，形成一个数据结构（包含多个函数指针），
 这样，通用文件系统访问接口层只需要访问文件系统抽象层，而不需要关心具体文件系统的实现细节和接口。
*/

struct inode;
struct stat;
struct dirent;

/*
 进程打开文件表对应的数据结构
 file & dir 接口层定义了进程在内核中直接访问的文件相关信息，这定义在 file 数据结构中
 这个结构并不是文件系统的结构，只是描述进程使用文件的情况
*/
struct file
{
    // 访问文件的执行状态
    enum
    {
        FD_NONE, FD_INIT, FD_OPENED, FD_CLOSED,
    } status;
    // 文件是否可读
    bool readable;
    // 文件是否可写
    bool writable;
    // 文件在 filemap 中的索引值
    int fd;
    // 访问文件的当前位置，也就是上次读取完的地方，用于记录分多次读取
    off_t pos;
    // 该文件对应的内存 inode 指针，之前 open 时通过搜索 entry 目录项获取到的，后续可以直接访问
    struct inode *node;
    // 文件打开计数
    int open_count;
};

void fd_array_init(struct file *fd_array);
void fd_array_open(struct file *file);
void fd_array_close(struct file *file);
void fd_array_dup(struct file *to, struct file *from);
bool file_testfd(int fd, bool readable, bool writable);

int file_open(char *path, uint32_t open_flags);
int file_close(int fd);
int file_read(int fd, void *base, size_t len, size_t *copied_store);
int file_write(int fd, void *base, size_t len, size_t *copied_store);
int file_seek(int fd, off_t pos, int whence);
int file_fstat(int fd, struct stat *stat);
int file_fsync(int fd);
int file_getdirentry(int fd, struct dirent *dirent);
int file_dup(int fd1, int fd2);
int file_pipe(int fd[]);
int file_mkfifo(const char *name, uint32_t open_flags);
int file_fchdir(int fd);

static inline int fopen_count(struct file *file)
{
    return file->open_count;
}

static inline int fopen_count_inc(struct file *file)
{
    file->open_count += 1;
    return file->open_count;
}

static inline int fopen_count_dec(struct file *file)
{
    file->open_count -= 1;
    return file->open_count;
}

#endif /* !__KERN_FS_FILE_H__ */


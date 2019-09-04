#include "defs.h"
#include "string.h"
#include "vfs.h"
#include "proc.h"
#include "file.h"
#include "unistd.h"
#include "iobuf.h"
#include "inode.h"
#include "stat.h"
#include "dirent.h"
#include "error.h"
#include "assert.h"
#include "stdio.h"
#include "slab.h"

#define testfd(fd)                          ((fd) >= 0 && (fd) < FILES_STRUCT_NENTRY)

// get_fd_array - get current process's open files table
static struct file *get_fd_array(void)
{
    struct files_struct *filesp = current->filesp;
    assert(filesp != NULL && files_count(filesp) > 0);
    return filesp->fd_array;
}

// fd_array_init - initialize the open files table
void fd_array_init(struct file *fd_array)
{
    int fd = 0;
    struct file *file = fd_array;
    int entry = FILES_STRUCT_NENTRY;
    for (fd = 0; fd < entry; fd ++, file ++)
    {
        file->open_count = 0;
        file->status = FD_NONE;
        file->fd = fd;
    }
}

// fs_array_alloc - allocate a free file item (with FD_NONE status) in open files table
static int fd_array_alloc(int fd, struct file **file_store)
{
//    panic("debug");
    // 从当前进程文件系统资源里分配，最多只有 FILES_STRUCT_NENTRY = 0x91 个
    struct file *file = get_fd_array();
    if (fd == NO_FD)
    {
        for (fd = 0; fd < FILES_STRUCT_NENTRY; fd ++, file ++)
        {
            if (file->status == FD_NONE)
            {
                goto found;
            }
        }
        return -E_MAX_OPEN;
    }
    else
    {
        if (testfd(fd))
        {
            file += fd;
            if (file->status == FD_NONE)
            {
                goto found;
            }
            return -E_BUSY;
        }
        return -E_INVAL;
    }
found:
    assert(fopen_count(file) == 0);
    file->status = FD_INIT;
    file->node = NULL;
    *file_store = file;
    return 0;
}

// fd_array_free - free a file item in open files table
static void fd_array_free(struct file *file)
{
    assert(file->status == FD_INIT || file->status == FD_CLOSED);
    assert(fopen_count(file) == 0);
    if (file->status == FD_CLOSED)
    {
        vfs_close(file->node);
    }
    file->status = FD_NONE;
}

static void fd_array_acquire(struct file *file)
{
    assert(file->status == FD_OPENED);
    fopen_count_inc(file);
}

// fd_array_release - file's open_count--; if file's open_count-- == 0 , then call fd_array_free to free this file item
static void fd_array_release(struct file *file)
{
    assert(file->status == FD_OPENED || file->status == FD_CLOSED);
    assert(fopen_count(file) > 0);
    if (fopen_count_dec(file) == 0)
    {
        fd_array_free(file);
    }
}

// fd_array_open - file's open_count++, set status to FD_OPENED
void fd_array_open(struct file *file)
{
    assert(file->status == FD_INIT && file->node != NULL);
    file->status = FD_OPENED;
    fopen_count_inc(file);
}

// fd_array_close - file's open_count--; if file's open_count-- == 0 , then call fd_array_free to free this file item
void fd_array_close(struct file *file)
{
    assert(file->status == FD_OPENED);
    assert(fopen_count(file) > 0);
    file->status = FD_CLOSED;
    
    // 文件没有进程打开时，可以释放文件资源
    if (fopen_count_dec(file) == 0)
    {
        fd_array_free(file);
    }
}

//fs_array_dup - duplicate file 'from'  to file 'to'
void fd_array_dup(struct file *to, struct file *from)
{
    //cprintf("[fd_array_dup]from fd=%d, to fd=%d\n",from->fd, to->fd);
    assert(to->status == FD_INIT && from->status == FD_OPENED);
    to->pos = from->pos;
    to->readable = from->readable;
    to->writable = from->writable;
    struct inode *node = from->node;
    inode_ref_inc(node);
    inode_open_inc(node);
    to->node = node;
    fd_array_open(to);
}

// fd2file - use fd as index of fd_array, return the array item (file)
static inline int fd2file(int fd, struct file **file_store)
{
    if (testfd(fd))
    {
        struct file *file = get_fd_array() + fd;
        if (file->status == FD_OPENED && file->fd == fd)
        {
            *file_store = file;
            return 0;
        }
    }
    return -E_INVAL;
}

// file_testfd - test file is readble or writable?
bool file_testfd(int fd, bool readable, bool writable)
{
    int ret = 0;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0)
    {
        return 0;
    }
    if (readable && !file->readable)
    {
        return 0;
    }
    if (writable && !file->writable)
    {
        return 0;
    }
    return 1;
}

// open file
int file_open(char *path, uint32_t open_flags)
{
    bool readable = 0, writable = 0;
    switch (open_flags & O_ACCMODE)
    {
        case O_RDONLY:
            readable = 1;
            break;
        case O_WRONLY:
            writable = 1;
            break;
        case O_RDWR:
            readable = writable = 1;
            break;
        default:
            return -E_INVAL;
    }

    int ret = 0;
    struct file *file;
    /*
     从当前进程文件系统资源里分配，分配一个空闲的 file 数据结构变量 file 在文件系统抽象层的处理中，
     要给这个即将打开的文件分配一个 file 数据结构的变量，这个变量其实是当前进程的打开文件数组
     current->fs_struct->filemap[] 中的一个空闲元素（即还没用于一个打开的文件），而这个元素的
     索引值就是最终要返回到用户进程并赋值给变量 fd
    */
    if ((ret = fd_array_alloc(NO_FD, &file)) != 0)
    {
        return ret;
    }

    struct inode *node;
    /*
     根据 path 文件路径找到对应的文件 inode 节点, 进一步调用 vfs_open 函数来找到 path 指出的文件
     所对应的基于 inode 数据结构的 VFS 索引节点 node。
     vfs_open 函数需要完成两件事情：通过 vfs_lookup 找到 path 对应文件的inode；
     调用 vop_open 函数打开文件。
    */
    if ((ret = vfs_open(path, open_flags, &node)) != 0)
    {
        fd_array_free(file);
        return ret;
    }

    file->pos = 0;
    if (open_flags & O_APPEND)
    {
        struct stat __stat, *stat = &__stat;
        
        assert(node != NULL && node->in_ops != NULL && node->in_ops->vop_fstat != NULL);
        inode_check(node, "fstat");
        
        if ((ret = node->in_ops->vop_fstat(node, stat)) != 0)
        {
            vfs_close(node);
            fd_array_free(file);
            return ret;
        }
        file->pos = stat->st_size;
    }

    // 搜索到文件 inode 节点后直接放到 file 里，方便进程后续读取
    file->node = node;
    file->readable = readable;
    file->writable = writable;
    fd_array_open(file);
//    cprintf("file_open, fd = %d, name = %s, path = %s\n", file->fd, file->node->nodename, path);
    return file->fd;
}

// close file
int file_close(int fd)
{
    int ret = 0;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0)
    {
        return ret;
    }
    fd_array_close(file);
//    cprintf("file_close, fd = %d, name = %s\n", file->fd, file->node->nodename);
    return 0;
}

// read file
int file_read(int fd, void *base, size_t len, size_t *copied_store)
{
    int ret = 0;
    struct file *file;
    *copied_store = 0;
    if ((ret = fd2file(fd, &file)) != 0)
    {
        return ret;
    }
    if (!file->readable)
    {
        return -E_INVAL;
    }
    fd_array_acquire(file);

    struct iobuf __iob, *iob = iobuf_init(&__iob, base, len, file->pos);
    
    assert(file->node != NULL && file->node->in_ops != NULL && file->node->in_ops->vop_read != NULL);
    inode_check(file->node, "read");
    ret = file->node->in_ops->vop_read(file->node, iob);

    size_t copied = iobuf_used(iob);
    if (file->status == FD_OPENED)
    {
        file->pos += copied;
    }
    *copied_store = copied;
    fd_array_release(file);
//    cprintf("file_read, fd = %d, name = %s, len = %d\n", file->fd, file->node->name, len);
    return ret;
}

// write file
int file_write(int fd, void *base, size_t len, size_t *copied_store)
{
    int ret = 0;
    struct file *file;
    *copied_store = 0;
    if ((ret = fd2file(fd, &file)) != 0)
    {
        return ret;
    }
    if (!file->writable)
    {
        return -E_INVAL;
    }
    fd_array_acquire(file);

    struct iobuf __iob, *iob = iobuf_init(&__iob, base, len, file->pos);
    
    assert(file->node != NULL && file->node->in_ops != NULL && file->node->in_ops->vop_write != NULL);
    inode_check(file->node, "write");
    ret = file->node->in_ops->vop_write(file->node, iob);

    size_t copied = iobuf_used(iob);
    if (file->status == FD_OPENED)
    {
        // 调整文件 pos 当前位置
        file->pos += copied;
    }
    *copied_store = copied;
    fd_array_release(file);
//    cprintf("file_write, fd = %d, name = %s, len = %d\n", file->fd, file->node->name, len);
    return ret;
}

// seek file
int file_seek(int fd, off_t pos, int whence)
{
    struct stat __stat, *stat = &__stat;
    int ret = 0;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0)
    {
        return ret;
    }
    fd_array_acquire(file);

    switch (whence)
    {
        case LSEEK_SET:
            // 从文件最开始出 seek
            break;
        case LSEEK_CUR:
            // 从文件当前位置 seek
            pos += file->pos;
            break;
        case LSEEK_END:
            // 从文件末尾开始 seek，这里需要先获取文件大小
            assert(file->node != NULL && file->node->in_ops != NULL && file->node->in_ops->vop_fstat != NULL);
            inode_check(file->node, "fstat");
            if ((ret = file->node->in_ops->vop_fstat(file->node, stat)) != 0)
            {
                pos += stat->st_size;
            }
            break;
        default:
            ret = -E_INVAL;
    }

    if (ret == 0)
    {
        assert(file->node != NULL && file->node->in_ops != NULL && file->node->in_ops->vop_tryseek != NULL);
        inode_check(file->node, "tryseek");
        if ((ret = file->node->in_ops->vop_tryseek(file->node, pos)) == 0)
        {
            file->pos = pos;
        }
//        cprintf("file_seek, fd = %d, name = %s, pos = %d, whence = %d, ret = %d\n", fd, file->node->nodename, pos, whence, ret);
    }
    fd_array_release(file);
    return ret;
}

// stat file
int file_fstat(int fd, struct stat *stat)
{
    int ret = 0;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0)
    {
        return ret;
    }
    fd_array_acquire(file);
    
    assert(file->node != NULL && file->node->in_ops != NULL && file->node->in_ops->vop_fstat != NULL);
    inode_check(file->node, "fstat");
    ret = file->node->in_ops->vop_fstat(file->node, stat);
    fd_array_release(file);
//    cprintf("file_fstat, fd = %d, name = %s\n", file->fd, file->node->name);
    return ret;
}

// sync file
int file_fsync(int fd)
{
    int ret = 0;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0)
    {
        return ret;
    }
    fd_array_acquire(file);
    
    assert(file->node != NULL && file->node->in_ops != NULL && file->node->in_ops->vop_fsync != NULL);
    inode_check(file->node, "fsync");
    ret = file->node->in_ops->vop_fsync(file->node);
    fd_array_release(file);
    cprintf("file_fsync, fd = %d, name = %s\n", file->fd, file->node->nodename);
    return ret;
}

// get file entry in DIR
int file_getdirentry(int fd, struct dirent *direntp)
{
    int ret = 0;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0)
    {
        return ret;
    }
    fd_array_acquire(file);

    struct iobuf __iob, *iob = iobuf_init(&__iob, direntp->name, sizeof(direntp->name), direntp->offset);
    
    assert(file->node != NULL && file->node->in_ops != NULL && file->node->in_ops->vop_getdirentry != NULL);
    inode_check(file->node, "getdirentry");
    if ((ret = file->node->in_ops->vop_getdirentry(file->node, iob)) == 0)
    {
        direntp->offset += iobuf_used(iob);
    }
    fd_array_release(file);
//    cprintf("file_getdirentry, fd = %d, name = %s\n", file->fd, file->node->nodename);
    return ret;
}

// duplicate file
int file_dup(int fd1, int fd2)
{
    int ret = 0;
    struct file *file1, *file2;
    if ((ret = fd2file(fd1, &file1)) != 0)
    {
        return ret;
    }
    if ((ret = fd_array_alloc(fd2, &file2)) != 0)
    {
        return ret;
    }
    fd_array_dup(file2, file1);
    return file2->fd;
}

int file_pipe(int fd[])
{
    int ret;
    struct file *file[2] = { NULL, NULL };
    if ((ret = fd_array_alloc(NO_FD, &file[0])) != 0)
    {
        goto failed_cleanup;
    }
    if ((ret = fd_array_alloc(NO_FD, &file[1])) != 0)
    {
        goto failed_cleanup;
    }
    
    if ((ret = pipe_open(&(file[0]->node), &(file[1]->node))) != 0)
    {
        goto failed_cleanup;
    }
    file[0]->pos = 0;
    file[0]->readable = 1;
    file[0]->writable = 0;
    fd_array_open(file[0]);
    
    file[1]->pos = 0;
    file[1]->readable = 0;
    file[1]->writable = 1;
    fd_array_open(file[1]);
    
    fd[0] = file[0]->fd;
    fd[1] = file[1]->fd;
    return 0;
    
failed_cleanup:
    if (file[0] != NULL)
    {
        fd_array_free(file[0]);
    }
    if (file[1] != NULL)
    {
        fd_array_free(file[1]);
    }
    return ret;
}

int file_mkfifo(const char *__name, uint32_t open_flags)
{
    bool readonly = 0;
    switch (open_flags & O_ACCMODE)
    {
        case O_RDONLY:
            readonly = 1;
        case O_WRONLY:
            break;
        default:
            return -E_INVAL;
    }
    
    int ret;
    struct file *file;
    if ((ret = fd_array_alloc(NO_FD, &file)) != 0)
    {
        return ret;
    }
    
    char *name;
    const char *device = readonly ? "pipe:r_" : "pipe:w_";
    if ((name = stradd(device, __name)) == NULL)
    {
        ret = -E_NO_MEM;
        goto failed_cleanup_file;
    }
    
    if ((ret = vfs_open(name, open_flags, &(file->node))) != 0)
    {
        goto failed_cleanup_name;
    }
    file->pos = 0;
    file->readable = readonly;
    file->writable = !readonly;
    fd_array_open(file);
    kfree(name);
    return file->fd;
    
failed_cleanup_name:
    kfree(name);
failed_cleanup_file:
    fd_array_free(file);
    return ret;
}

int file_fchdir(int fd)
{
    int ret = 0;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0)
    {
        return ret;
    }

    ret = vfs_set_curdir(file->node);
    return ret;
}


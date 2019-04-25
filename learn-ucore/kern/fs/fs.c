#include "defs.h"
#include "kmalloc.h"
#include "sem.h"
#include "vfs.h"
#include "dev.h"
#include "file.h"
#include "sfs.h"
#include "inode.h"
#include "assert.h"

//called when init_main proc start
/*
 Ucore 文件系统介绍
 https://www.cnblogs.com/miachel-zheng/p/6795025.html
 
 应用程序操作文件（打开/创建/删除/读写），首先需要通过文件系统的通用文件系统访问接口层给
 用户空间提供的访问接口进入文件系统内部，接着由文件系统抽象层把访问请求转发给某一具体文件
 系统（比如 SFS 文件系统），具体文件系统（Simple FS文件系统层）把应用程序的访问请求转化
 为对磁盘上的 block 的处理请求，并通过外设接口层交给磁盘驱动例程来完成具体的磁盘操作。
*/
void fs_init(void)
{
    vfs_init();
    dev_init();
    sfs_init();
}

void fs_cleanup(void)
{
    vfs_cleanup();
}

void lock_files(struct files_struct *filesp)
{
    down(&(filesp->files_sem));
}

void unlock_files(struct files_struct *filesp)
{
    up(&(filesp->files_sem));
}

//Called when a new proc init
// 进程创建的时候，就会分配文件系统相关资源空间
struct files_struct *files_create(void)
{
    //cprintf("[files_create]\n");
//    static_assert((int)FILES_STRUCT_NENTRY > 128);
    struct files_struct *filesp;
    // filesp 指向一个 4k 的空间
    if ((filesp = kmalloc(sizeof(struct files_struct) + FILES_STRUCT_BUFSIZE)) != NULL)
    {
        filesp->pwd = NULL;
        // 设置 fd_array 起始地址，文件 fd 个数 FILES_STRUCT_NENTRY = 0x91
        filesp->fd_array = (void *)(filesp + 1);
        filesp->files_count = 0;
        sem_init(&(filesp->files_sem), 1);
        fd_array_init(filesp->fd_array);
    }
    return filesp;
}

//Called when a proc exit
void files_destroy(struct files_struct *filesp)
{
//    cprintf("[files_destroy]\n");
    assert(filesp != NULL && files_count(filesp) == 0);
    if (filesp->pwd != NULL)
    {
        inode_ref_dec(filesp->pwd);
    }
    int i;
    struct file *file = filesp->fd_array;
    for (i = 0; i < FILES_STRUCT_NENTRY; i ++, file ++)
    {
        if (file->status == FD_OPENED)
        {
            fd_array_close(file);
        }
        assert(file->status == FD_NONE);
    }
    kfree(filesp);
}

void files_closeall(struct files_struct *filesp)
{
//    cprintf("[files_closeall]\n");
    assert(filesp != NULL && files_count(filesp) > 0);
    int i;
    struct file *file = filesp->fd_array;
    //skip the stdin & stdout
    for (i = 2, file += 2; i < FILES_STRUCT_NENTRY; i ++, file ++)
    {
        if (file->status == FD_OPENED)
        {
            fd_array_close(file);
        }
    }
}

int dup_fs(struct files_struct *to, struct files_struct *from)
{
//    cprintf("[dup_fs]\n");
    assert(to != NULL && from != NULL);
    assert(files_count(to) == 0 && files_count(from) > 0);
    if ((to->pwd = from->pwd) != NULL)
    {
        inode_ref_inc(to->pwd);
    }
    
    int i;
    struct file *to_file = to->fd_array, *from_file = from->fd_array;
    for (i = 0; i < FILES_STRUCT_NENTRY; i ++, to_file ++, from_file ++)
    {
        if (from_file->status == FD_OPENED)
        {
            /* alloc_fd first */
            to_file->status = FD_INIT;
            fd_array_dup(to_file, from_file);
        }
    }
    return 0;
}


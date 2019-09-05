#ifndef __KERN_FS_FS_H__
#define __KERN_FS_FS_H__

#include "defs.h"
#include "mmu.h"
#include "sem.h"
#include "atomic.h"

#define SECTSIZE            512
#define PAGE_NSECT          (PGSIZE / SECTSIZE)

/*
 Linux 文件系统详解
 https://www.cnblogs.com/alantu2018/p/8461749.html
*/

/*
 磁盘挂载顺序要和代码定义的保持一致，不然会访问出错
 
 qemu 模拟启动时命令如下
 qemu-system-i386 -S -s -parallel stdio -m 512M -drive file=bin/ucore.img -drive file=bin/swap.img -drive file=bin/sfs.img
 
 这里有连续 3 个 -drive 参数，分别代表第 1，2，3 个分区（索引 0，1，2）
 第 1 个分区是 ucore.img，这个是主分区，上面安装有操作系统引导程序 bootloader 和操作系统内核本身
 第 2 个分区是 swap.img，这个是交换分区，虚拟内存使用
 第 3 个分区是 sfs.img，Simple file system 文件系统，上面安装一些用户程序
 
 其中第 1 个 -drive 位置不能动，必须是第 1 个，否则加载不到内核，第 2，3 两个 -drive 可以调换位置
 但调换之后下面的宏定义也要对应修改才行
 
 下面的指令，可以挂载 4 个磁盘分区，qemu 最大也只能挂载 4 个，开启 smp，最后一个分区是 fatfs 文件系统
 qemu-system-i386 -S -s -parallel stdio -smp 16,cores=2,threads=2,sockets=4 -m 512M -drive file=bin/ucore.img -drive file=bin/swap.img -drive file=bin/sfs.img -drive file=fat.img
*/
#define UCORE_DEV_NO        0
#define SWAP_DEV_NO         1
#define DISK0_DEV_NO        2
#define DISK1_DEV_NO        3

void fs_init(void);
void fs_cleanup(void);

struct inode;
struct file;

/*
 * process's file related informaction
 */
/*
 进程访问文件的数据接口，记录当前进程的工作目录，打开的一些文件
 当创建一个进程后，该进程的 files_struct 将会被初始化或复制父进程的 files_struct。
 当用户进程打开一个文件时，将从 filemap 数组中取得一个空闲 file 项，然后会把此 file 的成员
 变量 node 指针指向一个代表此文件的 inode 的起始地址。
*/
struct files_struct
{
    // 进程当前工作目录的内存 inode 指针
    struct inode *pwd;      // inode of present working directory
    // 进程打开文件的数组
    struct file *fd_array;  // opened files array
    // 打开文件的个数
    int files_count;        // the number of opened files
    // 确保对进程控制块中 files_struct 的互斥访问
    semaphore_t files_sem;  // lock protect sem
};

// 4096 - 24 = 4072 = 0xFE8
#define FILES_STRUCT_BUFSIZE    (PGSIZE - sizeof(struct files_struct))
// 4072 / 28 = 145 = 0x91
#define FILES_STRUCT_NENTRY     (FILES_STRUCT_BUFSIZE / sizeof(struct file))

void lock_files(struct files_struct *filesp);
void unlock_files(struct files_struct *filesp);

struct files_struct *files_create(void);
void files_destroy(struct files_struct *filesp);
void files_closeall(struct files_struct *filesp);
int dup_files(struct files_struct *to, struct files_struct *from);
int dup_fs(struct files_struct *to, struct files_struct *from);

static inline int files_count(struct files_struct *filesp)
{
    return filesp->files_count;
}

static inline int files_count_inc(struct files_struct *filesp)
{
    filesp->files_count += 1;
    return filesp->files_count;
}

static inline int files_count_dec(struct files_struct *filesp)
{
    filesp->files_count -= 1;
    return filesp->files_count;
}

#endif /* !__KERN_FS_FS_H__ */


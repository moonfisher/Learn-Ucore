#ifndef __KERN_FS_DEVS_DEV_H__
#define __KERN_FS_DEVS_DEV_H__

#include "defs.h"

struct inode;
struct iobuf;

/*
 * Filesystem-namespace-accessible device.
 * d_io is for both reads and writes; the iobuf will indicates the direction.
 */
/*
 I / O 设备数据结构和相关操作
 
 device 这一层的作用，就是对硬件设备的抽象，不同的硬盘驱动，会提供不同的 IO 接口，内核认为这种
 杂乱的接口，不利于管理，需要把这些接口抽象一下，形成一个统一的对外接口，这样，不管你是什么硬盘，
 什么驱动，对外而言，它们所提供的 IO 接口没什么区别，都一视同仁的被看作块设备来处理。

 操作系统读取硬盘的时候，不会一个个扇区地读取，这样效率太低，而是一次性连续读取多个扇区，
 即一次性读取一个"块"（block）。这种由多个扇区组成的"块"，是文件存取的最小单位。"块"的大小，
 最常见的是 4KB，即连续八个 sector组成一个 block。
 
 device 这个数据结构只有当 inode 表示设备时才会有用
 d_blocks 表示设备占据的数据块个数
 d_blocksize 表示数据占据的数据块大小
*/
struct device
{
    int dvnum; // debtab中的索引
    char name[10];
    // 设备块数 = 磁盘扇区总数 ( ide_init 函数里获取 ) / (块大小 / 扇区大小)
    // = (磁盘扇区总数 * 扇区大小) / 块大小
    // block = 262144 / (4096 / 512) = 32768 = 0x8000
    size_t d_blocks;    // 设备占用的数据块个数，一块的大小是 4k
    size_t d_blocksize; // 数据块的大小，默认 4k，和内存页面一样大
    int (*d_init) (struct device *dev);
    int (*d_open)(struct device *dev, uint32_t open_flags, uint32_t arg2);
    int (*d_close)(struct device *dev);
    int (*d_io)(struct device *dev, struct iobuf *iob, bool write);
    int (*d_write)(struct device *dev, char *buff, int len);
    int (*d_read)(struct device *dev, char *buff, int len);
    int (*d_ioctl)(struct device *dev, int op, void *arg1, void *arg2);
    //tcpinit, dginit中设置
    char *dvioblk;  // ethblk,tcb , dgblk
    //在配置文件中设定
    int dvminor; // eth中的索引,tcb中的索引,upqs中的索引
};

void dev_init(void);
struct inode *dev_create_inode(void);

#endif /* !__KERN_FS_DEVS_DEV_H__ */


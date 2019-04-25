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
 操作系统读取硬盘的时候，不会一个个扇区地读取，这样效率太低，而是一次性连续读取多个扇区，
 即一次性读取一个"块"（block）。这种由多个扇区组成的"块"，是文件存取的最小单位。"块"的大小，
 最常见的是 4KB，即连续八个 sector组成一个 block。
 
 device 这个数据结构只有当 inode 表示设备时才会有用
 d_blocks 表示设备占据的数据块个数
 d_blocksize 表示数据占据的数据块大小
*/
struct device
{
    size_t d_blocks;    // 设备占用的数据块个数
    size_t d_blocksize; // 数据块的大小
    int (*d_open)(struct device *dev, uint32_t open_flags);
    int (*d_close)(struct device *dev);
    int (*d_io)(struct device *dev, struct iobuf *iob, bool write);
    int (*d_ioctl)(struct device *dev, int op, void *data);
};

void dev_init(void);
struct inode *dev_create_inode(void);

#endif /* !__KERN_FS_DEVS_DEV_H__ */


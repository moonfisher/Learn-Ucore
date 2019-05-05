#include "defs.h"
#include "mmu.h"
#include "sem.h"
#include "ide.h"
#include "inode.h"
#include "kmalloc.h"
#include "dev.h"
#include "vfs.h"
#include "iobuf.h"
#include "error.h"
#include "assert.h"

#define DISK0_BLKSIZE                   PGSIZE  // 4096
#define DISK0_BUFSIZE                   (4 * DISK0_BLKSIZE)
#define DISK0_BLK_NSECT                 (DISK0_BLKSIZE / SECTSIZE)  // 8

// 磁盘缓冲区，大小 4 个 block，16k
static char *disk0_buffer;
static semaphore_t disk0_sem;

void lock_disk0(void)
{
    down(&(disk0_sem));
}

void unlock_disk0(void)
{
    up(&(disk0_sem));
}

int disk0_open(struct device *dev, uint32_t open_flags)
{
    return 0;
}

int disk0_close(struct device *dev)
{
    return 0;
}

/*
 从设备上读取数据
 blkno  起始 block 号
 nblks  要读取的 block 数
 */
void disk0_read_blks_nolock(uint32_t blkno, uint32_t nblks)
{
    int ret;
    uint32_t sectno = blkno * DISK0_BLK_NSECT, nsecs = nblks * DISK0_BLK_NSECT;
    if ((ret = ide_read_secs(DISK0_DEV_NO, sectno, disk0_buffer, nsecs)) != 0)
    {
        panic("disk0: read blkno = %d (sectno = %d), nblks = %d (nsecs = %d): 0x%08x.\n",
                blkno, sectno, nblks, nsecs, ret);
    }
}

/*
 写入数据到设备上
 blkno  起始 block 号
 nblks  要写入 block 数
 */
void disk0_write_blks_nolock(uint32_t blkno, uint32_t nblks)
{
    int ret;
    uint32_t sectno = blkno * DISK0_BLK_NSECT, nsecs = nblks * DISK0_BLK_NSECT;
    if ((ret = ide_write_secs(DISK0_DEV_NO, sectno, disk0_buffer, nsecs)) != 0)
    {
        panic("disk0: write blkno = %d (sectno = %d), nblks = %d (nsecs = %d): 0x%08x.\n",
                blkno, sectno, nblks, nsecs, ret);
    }
}

int disk0_io(struct device *dev, struct iobuf *iob, bool write)
{
    off_t offset = iob->io_offset;
    size_t resid = iob->io_resid;
    uint32_t blkno = offset / DISK0_BLKSIZE;
    uint32_t nblks = resid / DISK0_BLKSIZE;

    /* don't allow I/O that isn't block-aligned */
    // 磁盘并不支持几个字节几个字节这样读写，必须整块的读写
    if ((offset % DISK0_BLKSIZE) != 0 || (resid % DISK0_BLKSIZE) != 0)
    {
        return -E_INVAL;
    }

    /* don't allow I/O past the end of disk0 */
    // 读写不能超过磁盘容量
    if (blkno + nblks > dev->d_blocks)
    {
        return -E_INVAL;
    }

    /* read/write nothing ? */
    if (nblks == 0)
    {
        return 0;
    }

    lock_disk0();
    while (resid != 0)
    {
        size_t copied, alen = DISK0_BUFSIZE;
        if (write)
        {
            // 写入数据到设备之前，先把数据写入到缓冲区
            iobuf_move(iob, disk0_buffer, alen, 0, &copied);
            assert(copied != 0 && copied <= resid && copied % DISK0_BLKSIZE == 0);
            nblks = copied / DISK0_BLKSIZE;
            disk0_write_blks_nolock(blkno, nblks);
        }
        else
        {
            if (alen > resid)
            {
                alen = resid;
            }
            nblks = alen / DISK0_BLKSIZE;
            // 从设备读取数据，会先读到缓冲区，再从缓冲区拷贝到目的地址
            disk0_read_blks_nolock(blkno, nblks);
            iobuf_move(iob, disk0_buffer, alen, 1, &copied);
            assert(copied == alen && copied % DISK0_BLKSIZE == 0);
        }
        resid -= copied;
        blkno += nblks;
    }
    unlock_disk0();
    return 0;
}

int disk0_ioctl(struct device *dev, int op, void *data)
{
    return -E_UNIMP;
}

void disk0_device_init(struct device *dev)
{
    static_assert(DISK0_BLKSIZE % SECTSIZE == 0);
    
    if (!ide_device_valid(DISK0_DEV_NO))
    {
        panic("disk0 device isn't available.\n");
    }
    
    // 设备块数 = 磁盘扇区总数 / (块大小 / 扇区大小) = (磁盘扇区总数 * 扇区大小) / 块大小
    // block = 262144 / (4096 / 512) = 32768 = 0x8000
    dev->d_blocks = ide_device_size(DISK0_DEV_NO) / DISK0_BLK_NSECT;    // 0x8000
    dev->d_blocksize = DISK0_BLKSIZE;   // 每一块大小 4k，和内存分页一样大
    dev->d_open = disk0_open;
    dev->d_close = disk0_close;
    dev->d_io = disk0_io;
    dev->d_ioctl = disk0_ioctl;
    sem_init(&(disk0_sem), 1);

    static_assert(DISK0_BUFSIZE % DISK0_BLKSIZE == 0);
    
    // 磁盘缓冲区，大小 4 个 block，16k
    if ((disk0_buffer = kmalloc(DISK0_BUFSIZE)) == NULL)
    {
        panic("disk0 alloc buffer failed.\n");
    }
}

// 这里只是初始化了设备，挂载里虚拟设备节点，还没安装文件系统
void dev_init_disk0(void)
{
    struct inode *node;
    if ((node = dev_create_inode()) == NULL)
    {
        panic("disk0: dev_create_node.\n");
    }
    
    // 完成设置 inode 为设备文件，初始化设备文件
    // vop_info 它完成返回 in_info 这个联合体里 device 的地址
    disk0_device_init(device_vop_info(node));
    
    int ret;
    if ((ret = vfs_add_dev("disk0", node, 1)) != 0)
    {
        panic("disk0: vfs_add_dev: %e.\n", ret);
    }
}


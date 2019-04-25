#include "defs.h"
#include "stdio.h"
#include "dev.h"
#include "vfs.h"
#include "iobuf.h"
#include "inode.h"
#include "unistd.h"
#include "error.h"
#include "assert.h"

static int stdout_open(struct device *dev, uint32_t open_flags)
{
    if (open_flags != O_WRONLY)
    {
        return -E_INVAL;
    }
    return 0;
}

static int stdout_close(struct device *dev)
{
    return 0;
}

static int stdout_io(struct device *dev, struct iobuf *iob, bool write)
{
    if (write)
    {
        char *data = iob->io_base;
        for (; iob->io_resid != 0; iob->io_resid --)
        {
            cputchar(*data ++);
        }
        return 0;
    }
    return -E_INVAL;
}

static int stdout_ioctl(struct device *dev, int op, void *data)
{
    return -E_INVAL;
}

static void stdout_device_init(struct device *dev)
{
    dev->d_blocks = 0;  // stdout 不是磁盘，没有块
    dev->d_blocksize = 1;
    dev->d_open = stdout_open;
    dev->d_close = stdout_close;
    dev->d_io = stdout_io;
    dev->d_ioctl = stdout_ioctl;
}

void dev_init_stdout(void)
{
    struct inode *node;
    if ((node = dev_create_inode()) == NULL)
    {
        panic("stdout: dev_create_node.\n");
    }
    
    // 完成设置 inode 为设备文件，初始化设备文件
    // vop_info 它完成返回 in_info 这个联合体里 device 的地址
    stdout_device_init(device_vop_info(node));

    int ret;
    if ((ret = vfs_add_dev("stdout", node, 0)) != 0)
    {
        panic("stdout: vfs_add_dev: %e.\n", ret);
    }
}


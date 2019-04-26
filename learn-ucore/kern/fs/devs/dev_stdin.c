#include "defs.h"
#include "stdio.h"
#include "wait.h"
#include "sync.h"
#include "proc.h"
#include "sched.h"
#include "dev.h"
#include "vfs.h"
#include "iobuf.h"
#include "inode.h"
#include "unistd.h"
#include "error.h"
#include "assert.h"

#define STDIN_BUFSIZE               4096

static char stdin_buffer[STDIN_BUFSIZE];
static off_t p_rpos, p_wpos;
static wait_queue_t __wait_queue, *wait_queue = &__wait_queue;

void dev_stdin_write(char c)
{
    bool intr_flag;
    if (c != '\0')
    {
        local_intr_save(intr_flag);
        {
            stdin_buffer[p_wpos % STDIN_BUFSIZE] = c;
            if (p_wpos - p_rpos < STDIN_BUFSIZE)
            {
                p_wpos++;
            }
            
            // 收到数据先写到缓冲区了，然后唤醒等待处理数据的进程进行处理
            if (!wait_queue_empty(wait_queue))
            {
                wakeup_queue(wait_queue, WT_KBD, 1);
            }
        }
        local_intr_restore(intr_flag);
    }
}

static int dev_stdin_read(char *buf, size_t len)
{
    int ret = 0;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        for (; ret < len; ret ++, p_rpos ++)
        {
        try_again:
            if (p_rpos < p_wpos)
            {
                *buf ++ = stdin_buffer[p_rpos % STDIN_BUFSIZE];
            }
            else
            {
                // 如果缓冲区里没有数据，就先挂起当前进程等待，打开中断，然后先调度别的进程运行，
                // 等后续 io 数据回来之后，会再次中断将数据存到缓冲区，然后唤醒当前进程来读取数据
                wait_t __wait, *wait = &__wait;
                wait_current_set(wait_queue, wait, WT_KBD);
                local_intr_restore(intr_flag);

                schedule();

                local_intr_save(intr_flag);
                wait_current_del(wait_queue, wait);
                if (wait->wakeup_flags == WT_KBD)
                {
                    goto try_again;
                }
                break;
            }
        }
    }
    local_intr_restore(intr_flag);
    return ret;
}

static int stdin_open(struct device *dev, uint32_t open_flags)
{
    if (open_flags != O_RDONLY)
    {
        return -E_INVAL;
    }
    return 0;
}

static int stdin_close(struct device *dev)
{
    return 0;
}

static int stdin_io(struct device *dev, struct iobuf *iob, bool write)
{
    if (!write)
    {
        int ret;
        if ((ret = dev_stdin_read(iob->io_base, iob->io_resid)) > 0)
        {
            iob->io_resid -= ret;
        }
        return ret;
    }
    return -E_INVAL;
}

static int stdin_ioctl(struct device *dev, int op, void *data)
{
    return -E_INVAL;
}

static void stdin_device_init(struct device *dev)
{
    dev->d_blocks = 0;  // stdin 不是磁盘，没有块
    dev->d_blocksize = 1;
    dev->d_open = stdin_open;
    dev->d_close = stdin_close;
    dev->d_io = stdin_io;
    dev->d_ioctl = stdin_ioctl;

    /*
     stdin 相对于 stdout 多了一个输入缓冲区，需要额外的两个指针 p_rpos, p_wpos 分别记录当前读
     的位置和写的位置，当 p_rpos < p_wpos 时，说明当前有从键盘输入到缓冲区的数据但是还没有读到
     进程里，需要唤醒进程从缓冲区进行读操作，当 p_rpos = p_wpos 而进程发起读的系统调用时
     （如调用 c 库的 scanf），这时需要阻塞进程，等待键盘输入时产生中断唤醒对应进程。
    */
    p_rpos = p_wpos = 0;
    wait_queue_init(wait_queue);
}

void dev_init_stdin(void)
{
    struct inode *node;
    if ((node = dev_create_inode()) == NULL)
    {
        panic("stdin: dev_create_node.\n");
    }
    
    // 完成设置 inode 为设备文件，初始化设备文件
    // vop_info 它完成返回 in_info 这个联合体里 device 的地址
    stdin_device_init(device_vop_info(node));

    int ret;
    if ((ret = vfs_add_dev("stdin", node, 0)) != 0)
    {
        panic("stdin: vfs_add_dev: %e.\n", ret);
    }
}


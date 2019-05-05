#include "defs.h"
#include "stdio.h"
#include "string.h"
#include "vfs.h"
#include "inode.h"
#include "sem.h"
#include "kmalloc.h"
#include "error.h"

static semaphore_t bootfs_sem;
// 当前文件系统根目录 “/” 对应的 inode
static struct inode *bootfs_node = NULL;

extern void vfs_devlist_init(void);

// __alloc_fs - allocate memory for fs, and set fs type
struct fs *__alloc_fs(int type)
{
    struct fs *fs;
    if ((fs = kmalloc(sizeof(struct fs))) != NULL)
    {
        fs->fs_type = type;
    }
    return fs;
}

// vfs_init -  vfs initialize
// 虚拟设备列表初始化，只是完成了对 vfs 访问的信号量和 devlist 的初始化
void vfs_init(void)
{
    sem_init(&bootfs_sem, 1);
    vfs_devlist_init();
}

// lock_bootfs - lock  for bootfs
static void lock_bootfs(void)
{
    down(&bootfs_sem);
}

// ulock_bootfs - ulock for bootfs
static void unlock_bootfs(void)
{
    up(&bootfs_sem);
}

// change_bootfs - set the new fs inode 
static void change_bootfs(struct inode *node)
{
    struct inode *old;
    lock_bootfs();
    {
        old = bootfs_node;
        bootfs_node = node;
    }
    unlock_bootfs();
    if (old != NULL)
    {
        inode_ref_dec(old);
    }
}

// vfs_set_bootfs - change the dir of file system "disk0"
int vfs_set_bootfs(char *fsname)
{
    struct inode *node = NULL;
    if (fsname != NULL)
    {
        char *s;
        if ((s = strchr(fsname, ':')) == NULL || s[1] != '\0')
        {
            return -E_INVAL;
        }
        int ret;
        if ((ret = vfs_chdir(fsname)) != 0)
        {
            return ret;
        }
        if ((ret = vfs_get_curdir(&node)) != 0)
        {
            return ret;
        }
    }
    change_bootfs(node);
    return 0;
}

// vfs_get_bootfs - get the inode of bootfs
int vfs_get_bootfs(struct inode **node_store)
{
    struct inode *node = NULL;
    if (bootfs_node != NULL)
    {
        lock_bootfs();
        {
            if ((node = bootfs_node) != NULL)
            {
                inode_ref_inc(bootfs_node);
            }
        }
        unlock_bootfs();
    }
    if (node == NULL)
    {
        return -E_NOENT;
    }
    *node_store = node;
    return 0;
}


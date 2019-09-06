#include "defs.h"
#include "stdio.h"
#include "string.h"
#include "vfs.h"
#include "inode.h"
#include "sem.h"
#include "slab.h"
#include "error.h"
#include "slab.h"

static semaphore_t bootfs_sem;
/*
 当前文件系统根目录 “/” 对应的 inode
 {
    in_info = {
        __device_info = {
            d_blocks = 3228318056,
            d_blocksize = 1,
            d_open = 0x0,
            d_close = 0x1,
            d_io = 0x1,
            d_ioctl = 0xc06c39cc
        },
        __sfs_inode_info = {
            din = 0xc06c3968,
            ino = 1,
            dirty = 0,
            reclaim_count = 1,
            sem = {
                value = 1,
                wait_queue = {
                    wait_head = {
                        prev = 0xc06c39cc,
                        next = 0xc06c39cc
                    }
                }
            },
            inode_link = {
                prev = 0xc06c3828,
                next = 0xc06c3828
            },
            hash_link = {
                prev = 0xc06d73c0,
                next = 0xc06d73c0
            }
        }
    },
    in_type = inode_type_sfs_inode_info,
    ref_count = 5,
    open_count = 0,
    in_fs = {
        fs_info = {
            __sfs_info = {
                super = {
                    magic = 797818410,
                    blocks = 32768,
                    unused_blocks = 32311,
                    info = "simple file system", '\000' <repeats 13 times>
                },
                dev = 0xc06c34b8,
                freemap = 0xc06c3870,
                super_dirty = 0,
                sfs_buffer = 0xc06d5000,
                fs_sem = {
                    value = 1,
                    wait_queue = {
                        wait_head = {
                            prev = 0xc06c3808,
                            next = 0xc06c3808
                        }
                    }
                },
                io_sem = {
                    value = 1,
                    wait_queue = {
                        wait_head = {
                            prev = 0xc06c3814,
                            next = 0xc06c3814
                        }
                    }
                },
                mutex_sem = {
                    value = 1,
                    wait_queue = {
                        wait_head = {
                            prev = 0xc06c3820,
                            next = 0xc06c3820
                        }
                    }
                },
                inode_list = {
                    prev = 0xc06c39d4,
                    next = 0xc06c39d4
                },
                hash_list = 0xc06d6000
            }
        },
        fs_type = fs_type_sfs_info,
        fs_sync = 0xc011203a <sfs_sync>,
        fs_get_root = 0xc0112180 <sfs_get_root>,
        fs_unmount = 0xc01121f0 <sfs_unmount>,
        fs_cleanup = 0xc01122b6 <sfs_cleanup>
    },
    in_ops = 0xc011ac40 <sfs_node_dirops>,
    nodename = "/", '\000' <repeats 254 times>
 }
*/
static struct inode *bootfs_node = NULL;

extern void vfs_devlist_init(void);

// __alloc_fs - allocate memory for fs, and set fs type
struct fs *__alloc_fs(int type)
{
    struct fs *fs;
    if ((fs = (struct fs *)kmalloc(sizeof(struct fs))) != NULL)
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
    file_system_type_list_init();
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

#define le2fstype(le, member)   to_struct((le), struct file_system_type, member)
static list_entry_t file_system_type_list;
static semaphore_t file_system_type_sem;

static void lock_file_system_type_list(void)
{
    down(&file_system_type_sem);
}

static void unlock_file_system_type_list(void)
{
    up(&file_system_type_sem);
}

static bool check_file_system_type_name_conflict(const char *name)
{
    list_entry_t *list = &file_system_type_list, *le = list;
    while ((le = list_next(le)) != list)
    {
        struct file_system_type *fstype = le2fstype(le, file_system_type_link);
        if (strcmp(fstype->name, name) == 0)
        {
            return 0;
        }
    }
    return 1;
}

void file_system_type_list_init(void)
{
    list_init(&file_system_type_list);
    sem_init(&file_system_type_sem, 1);
}

int register_filesystem(const char *name, int (*mount)(const char *devname, const char *source, const void *data))
{
    assert(name != NULL);
    if (strlen(name) > FS_MAX_DNAME_LEN)
    {
        return -E_TOO_BIG;
    }
    
    int ret = -E_NO_MEM;
    char *s_name;
    if ((s_name = strdup(name)) == NULL)
    {
        return ret;
    }
    
    struct file_system_type *fstype;
    if ((fstype = kmalloc(sizeof(struct file_system_type))) == NULL)
    {
        goto failed_cleanup_name;
    }
    
    ret = -E_EXISTS;
    lock_file_system_type_list();
    if (!check_file_system_type_name_conflict(s_name))
    {
        unlock_file_system_type_list();
        goto failed_cleanup_fstype;
    }
    fstype->name = s_name;
    fstype->mount = mount;
    
    list_add(&file_system_type_list, &(fstype->file_system_type_link));
    unlock_file_system_type_list();
    return 0;
    
failed_cleanup_fstype:
    kfree(fstype);
failed_cleanup_name:
    kfree(s_name);
    return ret;
}

int unregister_filesystem(const char *name)
{
    int ret = -E_EXISTS;
    lock_file_system_type_list();
    list_entry_t *list = &file_system_type_list, *le = list;
    while ((le = list_next(le)) != list)
    {
        struct file_system_type *fstype =
        le2fstype(le, file_system_type_link);
        if (strcmp(fstype->name, name) == 0)
        {
            list_del(le);
            kfree((char *)fstype->name);
            kfree(fstype);
            ret = 0;
            break;
        }
    }
    
    unlock_file_system_type_list();
    return ret;
}

int do_mount(const char *source, const char *devname, const char *fsname, const void *data)
{
    int ret = -E_EXISTS;
    lock_file_system_type_list();
    list_entry_t *list = &file_system_type_list, *le = list;
    while ((le = list_next(le)) != list)
    {
        struct file_system_type *fstype = le2fstype(le, file_system_type_link);
        if (strcmp(fstype->name, fsname) == 0)
        {
            assert(fstype->mount);
            ret = (fstype->mount)(devname, source, data);
            break;
        }
    }
    unlock_file_system_type_list();
    return ret;
}

int do_umount(const char *devname)
{
    return vfs_unmount(devname);
}

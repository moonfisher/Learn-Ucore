#include "defs.h"
#include "stdio.h"
#include "string.h"
#include "vfs.h"
#include "dev.h"
#include "inode.h"
#include "sem.h"
#include "list.h"
#include "slab.h"
#include "unistd.h"
#include "error.h"
#include "assert.h"

struct inode *sfs_get_root(struct fs *fs);
struct inode *pipe_get_root(struct fs *fs);
struct inode *ffs_get_root(struct fs *fs);

// device info entry in vdev_list
// 挂载在 vdev_list 下的虚拟设备节点
typedef struct
{
    // 虚拟设备名字 stdin，stdout，disk0 等
    const char *devname;
    // 虚拟设备对应的外设结构
    struct inode *devnode;
    // 虚拟设备对应的文件系统
    struct fs *fs;
    // 设备可挂载，可安装文件系统
    bool mountable;
    list_entry_t vdev_link;
} vfs_dev_t;

#define le2vdev(le, member)                         \
    to_struct((le), vfs_dev_t, member)

static list_entry_t vdev_list;     // device info list in vfs layer
static semaphore_t vdev_list_sem;

static void lock_vdev_list(void)
{
    down(&vdev_list_sem);
}

static void unlock_vdev_list(void)
{
    up(&vdev_list_sem);
}

void vfs_devlist_init(void)
{
    list_init(&vdev_list);
    sem_init(&vdev_list_sem, 1);
}

// vfs_cleanup - finally clean (or sync) fs
void vfs_cleanup(void)
{
    if (!list_empty(&vdev_list))
    {
        lock_vdev_list();
        {
            list_entry_t *list = &vdev_list, *le = list;
            while ((le = list_next(le)) != list)
            {
                vfs_dev_t *vdev = le2vdev(le, vdev_link);
                if (vdev->fs != NULL)
                {
                    (vdev->fs->fs_cleanup(vdev->fs));
                }
            }
        }
        unlock_vdev_list();
    }
}

/*
 * vfs_get_root - Given a device name (stdin, stdout, etc.), hand
 *                back an appropriate inode.
 */
/*
 从设备列表上，根据设备名字找到对应的设备，再根据设备上已经挂载的文件系统，找到对应的根目录
 设备上的文件系统挂载，最早是在 sfs_do_mount 里完成的
*/
int vfs_get_root(const char *devname, struct inode **node_store)
{
    assert(devname != NULL);
    int ret = -E_NO_DEV;
    if (!list_empty(&vdev_list))
    {
        lock_vdev_list();
        {
            list_entry_t *list = &vdev_list, *le = list;
            while ((le = list_next(le)) != list)
            {
                vfs_dev_t *vdev = le2vdev(le, vdev_link);
                if (strcmp(devname, vdev->devname) == 0)
                {
                    struct inode *found = NULL;
                    // fs 存在，说明当前设备上挂载了文件系统
                    if (vdev->fs != NULL)
                    {
                        if (vdev->fs->fs_type == fs_type_pipe_info)
                        {
                            found = pipe_get_root(vdev->fs);
                        }
                        else if (vdev->fs->fs_type == fs_type_sfs_info)
                        {
                            found = sfs_get_root(vdev->fs);
                        }
                        else if (vdev->fs->fs_type == fs_type_ffs_info)
                        {
                            found = ffs_get_root(vdev->fs);
                        }
                    }
                    // fs 不存在，说明当前设备上没有文件系统，可能是输入，输出，网卡等设备
                    else if (!vdev->mountable)
                    {
                        inode_ref_inc(vdev->devnode);
                        found = vdev->devnode;
                    }
                    
                    if (found != NULL)
                    {
                        ret = 0;
                        *node_store = found;
                    }
                    else
                    {
                        ret = -E_NA_DEV;
                    }
                    break;
                }
            }
        }
        unlock_vdev_list();
    }
    return ret;
}

/*
 * vfs_get_devname - Given a filesystem, hand back the name of the device it's mounted on.
 */
const char *vfs_get_devname(struct fs *fs)
{
    assert(fs != NULL);
    list_entry_t *list = &vdev_list, *le = list;
    while ((le = list_next(le)) != list)
    {
        vfs_dev_t *vdev = le2vdev(le, vdev_link);
        if (vdev->fs == fs)
        {
            return vdev->devname;
        }
    }
    return NULL;
}

/*
 * check_devname_confilct - Is there alreadily device which has the same name?
 */
static bool check_devname_conflict(const char *devname)
{
    list_entry_t *list = &vdev_list, *le = list;
    while ((le = list_next(le)) != list)
    {
        vfs_dev_t *vdev = le2vdev(le, vdev_link);
        if (strcmp(vdev->devname, devname) == 0)
        {
            return 0;
        }
    }
    return 1;
}

int vfs_sync(void)
{
    if (!list_empty(&vdev_list))
    {
        lock_vdev_list();
        {
            list_entry_t *list = &vdev_list, *le = list;
            while ((le = list_next(le)) != list)
            {
                vfs_dev_t *vdev = le2vdev(le, vdev_link);
                if (vdev->fs != NULL)
                {
                    vdev->fs->fs_sync(vdev->fs);
                }
            }
        }
        unlock_vdev_list();
    }
    return 0;
}

/*
* vfs_do_add - Add a new device to the VFS layer's device table.
*
* If "mountable" is set, the device will be treated as one that expects
* to have a filesystem mounted on it, and a raw device will be created
* for direct access.
*/
/*
 添加设备到虚拟文件设备列表里，devname 设备名字是不重复的，这里设备名字是唯一标识
 如果设置了 “mountable”，则设备将被视为预期的设备在其上安装文件系统，并创建原始设备直接访问。
*/
static int vfs_do_add(const char *devname, struct inode *devnode, struct fs *fs, bool mountable)
{
    assert(devname != NULL);
    assert((devnode == NULL && !mountable) || (devnode != NULL && ((devnode)->in_type == inode_type_device_info)));
    if (strlen(devname) > FS_MAX_DNAME_LEN)
    {
        return -E_TOO_BIG;
    }

    int ret = -E_NO_MEM;
    char *s_devname;
    if ((s_devname = strdup(devname)) == NULL)
    {
        return ret;
    }

    // 并不是直接挂载 inode 到 vdev_list，而是把 inode 封装到 vfs_dev_t 里再挂载
    vfs_dev_t *vdev;
    if ((vdev = (vfs_dev_t *)kmalloc(sizeof(vfs_dev_t))) == NULL)
    {
        goto failed_cleanup_name;
    }

    ret = -E_EXISTS;
    lock_vdev_list();
    if (!check_devname_conflict(s_devname))
    {
        unlock_vdev_list();
        goto failed_cleanup_vdev;
    }
    vdev->devname = s_devname;
    vdev->devnode = devnode;
    vdev->mountable = mountable;
    vdev->fs = fs;

    list_add(&vdev_list, &(vdev->vdev_link));
    unlock_vdev_list();
    
    if (devnode)
    {
        memset(devnode->nodename, 0, 256);
        memcpy(devnode->nodename, devname, strlen(devname));
    }
    return 0;

failed_cleanup_vdev:
    kfree(vdev);
failed_cleanup_name:
    kfree(s_devname);
    return ret;
}

/*
 * vfs_add_fs - Add a new fs,  by name. See  vfs_do_add information for the description of
 *              mountable.
 */
int vfs_add_fs(const char *devname, struct fs *fs)
{
    return vfs_do_add(devname, NULL, fs, 0);
}

/*
 * vfs_add_dev - Add a new device, by name. See  vfs_do_add information for the description of
 *               mountable.
 */
/*
 添加设备到虚拟文件设备列表里，devname 设备名字是不重复的，这里设备名字是唯一标识
 */
int vfs_add_dev(const char *devname, struct inode *devnode, bool mountable)
{
    return vfs_do_add(devname, devnode, NULL, mountable);
}

/*
 * find_mount - Look for a mountable device named DEVNAME.
 *              Should already hold vdev_list lock.
 */
static int find_mount(const char *devname, vfs_dev_t **vdev_store)
{
    assert(devname != NULL);
    list_entry_t *list = &vdev_list, *le = list;
    while ((le = list_next(le)) != list)
    {
        vfs_dev_t *vdev = le2vdev(le, vdev_link);
        if (vdev->mountable && strcmp(vdev->devname, devname) == 0)
        {
            *vdev_store = vdev;
            return 0;
        }
    }
    return -E_NO_DEV;
}

/*
 * vfs_mount - Mount a filesystem. Once we've found the device, call MOUNTFUNC to
 *             set up the filesystem and hand back a struct fs.
 *
 * The DATA argument is passed through unchanged to MOUNTFUNC.
 */
/*
 其中这里面最重要的就是对回调函数 sfs_do_mount(mountfunc) 的调用，sfs_do_mount 主要完成对 struct sfs
 数据结构的初始化，这里的 sfs 是 simple file system 的缩写
*/
int vfs_mount(const char *devname, const char *source, const void *data, int (*mountfunc)(struct device *dev, struct fs **fs_store))
{
    int ret;
    lock_vdev_list();
    vfs_dev_t *vdev;
    
    // 根据设备名字在虚拟设备链表上先找到对应的设备
    if ((ret = find_mount(devname, &vdev)) != 0)
    {
        goto out;
    }
    if (vdev->fs != NULL)
    {
        ret = -E_BUSY;
        goto out;
    }
    assert(vdev->devname != NULL && vdev->mountable);

    struct device *dev = device_vop_info(vdev->devnode);
    if ((ret = mountfunc(dev, &(vdev->fs))) == 0)
    {
        assert(vdev->fs != NULL);
        cprintf("vfs: mount %s.\n", vdev->devname);
    }

out:
    unlock_vdev_list();
    return ret;
}

/*
 * vfs_unmount - Unmount a filesystem/device by name.
 *               First calls FSOP_SYNC on the filesystem; then calls FSOP_UNMOUNT.
 */
int vfs_unmount(const char *devname)
{
    int ret;
    lock_vdev_list();
    vfs_dev_t *vdev;
    if ((ret = find_mount(devname, &vdev)) != 0)
    {
        goto out;
    }
    if (vdev->fs == NULL)
    {
        ret = -E_INVAL;
        goto out;
    }
    assert(vdev->devname != NULL && vdev->mountable);

    if ((ret = (vdev->fs->fs_sync(vdev->fs))) != 0)
    {
        goto out;
    }
    if ((ret = (vdev->fs->fs_unmount(vdev->fs))) == 0)
    {
        vdev->fs = NULL;
        cprintf("vfs: unmount %s.\n", vdev->devname);
    }

out:
    unlock_vdev_list();
    return ret;
}

/*
 * vfs_unmount_all - Global unmount function.
 */
int vfs_unmount_all(void)
{
    if (!list_empty(&vdev_list))
    {
        lock_vdev_list();
        {
            list_entry_t *list = &vdev_list, *le = list;
            while ((le = list_next(le)) != list)
            {
                vfs_dev_t *vdev = le2vdev(le, vdev_link);
                if (vdev->mountable && vdev->fs != NULL)
                {
                    int ret;
                    if ((ret = (vdev->fs->fs_sync(vdev->fs))) != 0)
                    {
                        cprintf("vfs: warning: sync failed for %s: %e.\n", vdev->devname, ret);
                        continue ;
                    }
                    if ((ret = (vdev->fs->fs_unmount(vdev->fs))) != 0)
                    {
                        cprintf("vfs: warning: unmount failed for %s: %e.\n", vdev->devname, ret);
                        continue ;
                    }
                    vdev->fs = NULL;
                    cprintf("vfs: unmount %s.\n", vdev->devname);
                }
            }
        }
        unlock_vdev_list();
    }
    return 0;
}

void vfs_print(void)
{
    if (!list_empty(&vdev_list))
    {
        cprintf("\n-------------------- vfs_print BEGIN --------------------\n");
        lock_vdev_list();
        {
            list_entry_t *list = &vdev_list, *le = list;
            while ((le = list_next(le)) != list)
            {
                vfs_dev_t *vdev = le2vdev(le, vdev_link);
                if (vdev)
                {
                    char *nodename = "";
                    if (vdev->devnode)
                    {
                        nodename = vdev->devnode->nodename;
                    }
                    
                    char *fsname = "";
                    if (vdev->fs)
                    {
                        fsname = vdev->fs->fsname;
                    }
                    
                    cprintf("name = %s, mountable = %d, nodename = %s, fsname = %s\n", vdev->devname, vdev->mountable, nodename, fsname);
                }
            }
        }
        unlock_vdev_list();
        cprintf("-------------------- vfs_print END --------------------\n");
    }
    return;
}

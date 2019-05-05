#include "defs.h"
#include "stdio.h"
#include "string.h"
#include "kmalloc.h"
#include "list.h"
#include "fs.h"
#include "vfs.h"
#include "dev.h"
#include "sfs.h"
#include "inode.h"
#include "iobuf.h"
#include "bitmap.h"
#include "error.h"
#include "assert.h"

int disk0_io(struct device *dev, struct iobuf *iob, bool write);

/*
 * sfs_sync - sync sfs's superblock and freemap in memroy into disk
 */
int sfs_sync(struct fs *fs)
{
    assert(fs != NULL && (fs->fs_type == fs_type_sfs_info));
    struct sfs_fs *sfs = &(fs->fs_info.__sfs_info);
    lock_sfs_fs(sfs);
    {
        list_entry_t *list = &(sfs->inode_list), *le = list;
        while ((le = list_next(le)) != list)
        {
            struct sfs_inode *sin = le2sin(le, inode_link);
            struct inode *node = info2node(sin, sfs_inode);
            assert(node != NULL && node->in_ops != NULL && node->in_ops->vop_fsync != NULL);
            inode_check(node, "fsync");
            node->in_ops->vop_fsync(node);
        }
    }
    unlock_sfs_fs(sfs);

    int ret;
    if (sfs->super_dirty)
    {
        sfs->super_dirty = 0;
        if ((ret = sfs_sync_super(sfs)) != 0)
        {
            sfs->super_dirty = 1;
            return ret;
        }
        if ((ret = sfs_sync_freemap(sfs)) != 0)
        {
            sfs->super_dirty = 1;
            return ret;
        }
    }
    return 0;
}

/*
 * sfs_get_root - get the root directory inode  from disk (SFS_BLKN_ROOT,1)
 */
struct inode *sfs_get_root(struct fs *fs)
{
    struct inode *node;
    int ret;
    assert(fs != NULL && (fs->fs_type == fs_type_sfs_info));
    struct sfs_fs *sfs = &(fs->fs_info.__sfs_info);
    
    if ((ret = sfs_load_inode(sfs, &node, SFS_BLKN_ROOT)) != 0)
    {
        panic("load sfs root failed: %e", ret);
    }
    return node;
}

/*
 * sfs_unmount - unmount sfs, and free the memorys contain sfs->freemap/sfs_buffer/hash_liskt and sfs itself.
 */
int sfs_unmount(struct fs *fs)
{
    assert(fs != NULL && (fs->fs_type == fs_type_sfs_info));
    struct sfs_fs *sfs = &(fs->fs_info.__sfs_info);
    if (!list_empty(&(sfs->inode_list)))
    {
        return -E_BUSY;
    }
    assert(!sfs->super_dirty);
    bitmap_destroy(sfs->freemap);
    kfree(sfs->sfs_buffer);
    kfree(sfs->hash_list);
    kfree(sfs);
    return 0;
}

/*
 * sfs_cleanup - when sfs failed, then should call this function to sync sfs by calling sfs_sync
 *
 * NOTICE: nouse now.
 */
void sfs_cleanup(struct fs *fs)
{
    assert(fs != NULL && (fs->fs_type == fs_type_sfs_info));
    struct sfs_fs *sfs = &(fs->fs_info.__sfs_info);
    uint32_t blocks = sfs->super.blocks, unused_blocks = sfs->super.unused_blocks;
    cprintf("sfs: cleanup: '%s' (%d/%d/%d)\n", sfs->super.info,
            blocks - unused_blocks, unused_blocks, blocks);
    int i = 0, ret = 0;
    for (i = 0; i < 32; i ++)
    {
        if ((ret = (fs->fs_sync(fs))) == 0)
        {
            break;
        }
    }
    if (ret != 0)
    {
        warn("sfs: sync error: '%s': %e.\n", sfs->super.info, ret);
    }
}

/*
 * sfs_init_read - used in sfs_do_mount to read disk block(blkno, 1) directly.
 *
 * @dev:        the block device
 * @blkno:      the NO. of disk block
 * @blk_buffer: the buffer used for read
 *
 *      (1) init iobuf
 *      (2) read dev into iobuf
 */
int sfs_init_read(struct device *dev, uint32_t blkno, void *blk_buffer)
{
    struct iobuf __iob;
    struct iobuf *iob = iobuf_init(&__iob, blk_buffer, SFS_BLKSIZE, blkno * SFS_BLKSIZE);
//    return dev->d_io(dev, iob, 0);
    return disk0_io(dev, iob, 0);
}

/*
 * sfs_init_freemap - used in sfs_do_mount to read freemap data info in disk block(blkno, nblks) directly.
 *
 * @dev:        the block device
 * @bitmap:     the bitmap in memroy
 * @blkno:      the NO. of disk block
 * @nblks:      Rd number of disk block
 * @blk_buffer: the buffer used for read
 *
 *      (1) get data addr in bitmap
 *      (2) read dev into iobuf
 */
int sfs_init_freemap(struct device *dev, struct bitmap *freemap, uint32_t blkno, uint32_t nblks, void *blk_buffer)
{
    size_t len;
    void *data = bitmap_getdata(freemap, &len);
    assert(data != NULL && len == nblks * SFS_BLKSIZE);
    while (nblks != 0)
    {
        int ret;
        if ((ret = sfs_init_read(dev, blkno, data)) != 0)
        {
            return ret;
        }
        blkno ++;
        nblks --;
        data += SFS_BLKSIZE;
    }
    return 0;
}

/*
 * sfs_do_mount - mount sfs file system.
 *
 * @dev:        the block device contains sfs file system
 * @fs_store:   the fs struct in memroy
 */
/*
  在 sfs_do_mount 函数中，完成了加载位于硬盘上的 SFS 文件系统的超级块 superblock 和 freemap
  的工作。这样，在内存中就有了 SFS 文件系统的全局信息。
*/
int sfs_do_mount(struct device *dev, struct fs **fs_store)
{
    static_assert(SFS_BLKSIZE >= sizeof(struct sfs_super));
    static_assert(SFS_BLKSIZE >= sizeof(struct sfs_disk_inode));
    static_assert(SFS_BLKSIZE >= sizeof(struct sfs_disk_entry));

    if (dev->d_blocksize != SFS_BLKSIZE)
    {
        return -E_NA_DEV;
    }

    /* allocate fs structure */
    // 开始构造 fs 相关数据结构
    struct fs *fs;
    if ((fs = __alloc_fs(fs_type_sfs_info)) == NULL)
    {
        return -E_NO_MEM;
    }
    
    assert(fs != NULL && (fs->fs_type == fs_type_sfs_info));
    
    struct sfs_fs *sfs = &(fs->fs_info.__sfs_info);
    sfs->dev = dev;

    int ret = -E_NO_MEM;

    void *sfs_buffer;   // 文件读写缓冲区，大小 4k
    if ((sfs->sfs_buffer = sfs_buffer = kmalloc(SFS_BLKSIZE)) == NULL)
    {
        goto failed_cleanup_fs;
    }

    /* load and check superblock */
    /*
     第 0 个块（4K）是超级块（superblock struct sfs_super），它包含了关于文件系统的
     所有关键参数，当计算机被启动或文件系统被首次接触时，超级块的内容就会被装入内存。
     这些数据在 mksfs.c 里创建 sfs.img 的时候创建好
    */
    if ((ret = sfs_init_read(dev, SFS_BLKN_SUPER, sfs_buffer)) != 0)
    {
        goto failed_cleanup_sfs_buffer;
    }

    ret = -E_INVAL;

    struct sfs_super *super = sfs_buffer;
    if (super->magic != SFS_MAGIC)
    {
        cprintf("sfs: wrong magic in superblock. (%08x should be %08x).\n", super->magic, SFS_MAGIC);
        goto failed_cleanup_sfs_buffer;
    }
    if (super->blocks > dev->d_blocks)
    {
        cprintf("sfs: fs has %u blocks, device has %u blocks.\n", super->blocks, dev->d_blocks);
        goto failed_cleanup_sfs_buffer;
    }
    super->info[SFS_MAX_INFO_LEN] = '\0';
    sfs->super = *super;

    ret = -E_NO_MEM;

    uint32_t i;

    /* alloc and initialize hash list */
    list_entry_t *hash_list;
    if ((sfs->hash_list = hash_list = kmalloc(sizeof(list_entry_t) * SFS_HLIST_SIZE)) == NULL)
    {
        goto failed_cleanup_sfs_buffer;
    }
    for (i = 0; i < SFS_HLIST_SIZE; i ++)
    {
        list_init(hash_list + i);
    }

    /* load and check freemap */
    struct bitmap *freemap;
    uint32_t freemap_size_nbits = sfs_freemap_bits(super);
    if ((sfs->freemap = freemap = bitmap_create(freemap_size_nbits)) == NULL)
    {
        goto failed_cleanup_hash_list;
    }
    
    uint32_t freemap_size_nblks = sfs_freemap_blocks(super);
    if ((ret = sfs_init_freemap(dev, freemap, SFS_BLKN_FREEMAP, freemap_size_nblks, sfs_buffer)) != 0)
    {
        goto failed_cleanup_freemap;
    }

    uint32_t blocks = sfs->super.blocks, unused_blocks = 0;
    for (i = 0; i < freemap_size_nbits; i ++)
    {
        if (bitmap_test(freemap, i))
        {
            unused_blocks ++;
        }
    }
    assert(unused_blocks == sfs->super.unused_blocks);

    /* and other fields */
    sfs->super_dirty = 0;
    sem_init(&(sfs->fs_sem), 1);
    sem_init(&(sfs->io_sem), 1);
    sem_init(&(sfs->mutex_sem), 1);
    list_init(&(sfs->inode_list));
    cprintf("sfs: mount: '%s' (%d/%d/%d)\n", sfs->super.info, blocks - unused_blocks, unused_blocks, blocks);

    /* link addr of sync/get_root/unmount/cleanup funciton  fs's function pointers*/
    fs->fs_sync = sfs_sync;
    fs->fs_get_root = sfs_get_root;
    fs->fs_unmount = sfs_unmount;
    fs->fs_cleanup = sfs_cleanup;
    *fs_store = fs;
    return 0;

failed_cleanup_freemap:
    bitmap_destroy(freemap);
failed_cleanup_hash_list:
    kfree(hash_list);
failed_cleanup_sfs_buffer:
    kfree(sfs_buffer);
failed_cleanup_fs:
    kfree(fs);
    return ret;
}

/*
 挂载 disk0，并安装 sfs 文件系统
 */
int sfs_mount(const char *devname)
{
    return vfs_mount(devname, sfs_do_mount);
}


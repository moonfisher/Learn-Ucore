#include "defs.h"
#include "stdio.h"
#include "string.h"
#include "slab.h"
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
/*
 第 1 个块（4K）(sfs.img 文件第 2 个 4K 字节) 放了一个 root-dir 的 inode，用来记录根目录的
 相关信息。root-dir 是 SFS 文件系统的根结点，通过这个 root-dir 的 inode 信息就可以定位并查找到
 根目录下的所有文件信息。
 
 root 根目录下一共有 blocks = 0x18 = 24 个文件，direct 里只能放下 12 个索引，还有 12 个通过
 indirect 间接获取，indirect 地址是 0x8d * 4k = 0x8d000
 
 0018 0000 0200 0200 1800 0000 0300 0000
 0400 0000 1000 0000 1c00 0000 2a00 0000
 3600 0000 4400 0000 5000 0000 5c00 0000
 6800 0000 7400 0000 8000 0000 8d00 0000
 
 p /x *(node.in_info.__sfs_inode_info->din)
 {
    size = 0x1800,
    type = 0x2, (SFS_TYPE_DIR)
    nlinks = 0x2,
    blocks = 0x18,
    direct = {
        0x3, 0x4, 0x10, 0x1c, 0x2a, 0x36, 0x44, 0x50, 0x5c, 0x68, 0x74, 0x80
    },
    indirect = 0x8d
 }
 
 另外 12 文件索引放在这里
 x /32hx 0x8d000
 8c00 0000 9900 0000 a500 0000 b100 0000
 bd00 0000 ca00 0000 d600 0000 e200 0000
 ee00 0000 fa00 0000 0701 0000 1301 0000
 
 p /x *node
 {
    in_info = {
        __sfs_inode_info = {
            din = 0xc06bc410,
            ino = 0x1,
            dirty = 0x0,
            reclaim_count = 0x1,
            sem = {
                value = 0x1,
                wait_queue = {
                    wait_head = {
                        prev = 0xc06bc46c,
                        next = 0xc06bc46c
                    }
                }
            },
            inode_link = {
                prev = 0xc06bc3a0,
                next = 0xc06bc3a0
            },
            hash_link = {
                prev = 0xc06cc3c0,
                next = 0xc06cc3c0
            }
        }
    },
    in_type = 0x1235,
    ref_count = 0x1,
    open_count = 0x0,
    in_fs = 0xc06bc340,
    in_ops = 0xc0118720
 }
*/
struct inode *sfs_get_root(struct fs *fs)
{
    struct inode *node;
    assert(fs != NULL && (fs->fs_type == fs_type_sfs_info));
    struct sfs_fs *sfs = &(fs->fs_info.__sfs_info);
    
    int ret = sfs_load_inode(sfs, &node, SFS_BLKN_ROOT, "/");
    if (ret != 0)
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
    return dev->d_io(dev, iob, 0);
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

    void *sfs_buffer = (void *)kmalloc(SFS_BLKSIZE);   // 文件读写缓冲区，大小 4k
    if (sfs_buffer == NULL)
    {
        goto failed_cleanup_fs;
    }
    memset(sfs_buffer, 0, SFS_BLKSIZE);
    sfs->sfs_buffer = sfs_buffer;

    /* load and check superblock */
    /*
     第 0 号块（sfs.img 文件第 1 个 4K 字节）是超级块（superblock struct sfs_super），
     它包含了关于文件系统的关键参数，有多少块，空闲多少块没用。当计算机被启动或文件系统被首次
     接触时，超级块的内容就会被装入内存。
     这些数据在 mksfs.c 里创建 sfs.img 的时候创建好
     
     2abe 8d2f 0080 0000 ec7e 0000 7369 6d70
     6c65 2066 696c 6520 7379 7374 656d 0000
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
    
    // super->blocks        = 0x8000
    // super->unused_blocks = 0x7eec
    if (super->blocks > dev->d_blocks)
    {
        cprintf("sfs: fs has %u blocks, device has %u blocks.\n", super->blocks, dev->d_blocks);
        goto failed_cleanup_sfs_buffer;
    }
    super->info[SFS_MAX_INFO_LEN] = '\0';
    // 设置 super 超级块，这里是个结构体赋值，不是指针
    sfs->super = *super;

    ret = -E_NO_MEM;

    uint32_t i;

    /* alloc and initialize hash list */
    list_entry_t *hash_list = (list_entry_t *)kmalloc(sizeof(list_entry_t) * SFS_HLIST_SIZE);
    if ((sfs->hash_list = hash_list) == NULL)
    {
        goto failed_cleanup_sfs_buffer;
    }
    for (i = 0; i < SFS_HLIST_SIZE; i ++)
    {
        list_init(hash_list + i);
    }

    /* load and check freemap */
    /*
     第 2 号块（sfs.img 文件第 3 个 4K 字节）
     根据 SFS 中所有块的数量，记录块占用情况，用 1 个 bit 来表示一个块的占用和未被
     占用的情况。这个区域称为 SFS 的 freemap 区域，这将占用若干个块空间
     */
    uint32_t freemap_size_nbits = sfs_freemap_bits(super);
    struct bitmap *freemap = bitmap_create(freemap_size_nbits);
    if ((sfs->freemap = freemap) == NULL)
    {
        goto failed_cleanup_hash_list;
    }
    
    uint32_t freemap_size_nblks = sfs_freemap_blocks(super);
    ret = sfs_init_freemap(dev, freemap, SFS_BLKN_FREEMAP, freemap_size_nblks, sfs_buffer);
    if (ret != 0)
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

    memset(fs->fsname, 0, 256);
    memcpy(fs->fsname, "sfs", strlen("sfs"));
    
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


#include "defs.h"
#include "stdio.h"
#include "string.h"
#include "atomic.h"
#include "vfs.h"
#include "inode.h"
#include "error.h"
#include "assert.h"
#include "slab.h"

/* *
 * __alloc_inode - alloc a inode structure and initialize in_type
 * */
struct inode *__alloc_inode(int type)
{
    struct inode *node;
    if ((node = kmalloc(sizeof(struct inode))) != NULL)
    {
        node->in_type = type;
    }
    return node;
}

/* *
 * inode_init - initialize a inode structure
 * invoked by vop_init
 * */
void inode_init(struct inode *node, const struct inode_ops *ops, struct fs *fs)
{
    node->ref_count = 0;
    node->open_count = 0;
    node->in_ops = ops;
    node->in_fs = fs;
    memset(node->nodename, 0, 256);
    // 节点一创建好就设置引用计数为 1
    inode_ref_inc(node);
}

/* *
 * inode_kill - kill a inode structure
 * invoked by vop_kill
 * */
void inode_kill(struct inode *node)
{
    assert(inode_ref_count(node) == 0);
    assert(inode_open_count(node) == 0);
    kfree(node);
}

/* *
 * inode_ref_inc - increment ref_count
 * invoked by inode_ref_inc
 * */
int inode_ref_inc(struct inode *node)
{
    node->ref_count += 1;
    return node->ref_count;
}

/* *
 * inode_ref_dec - decrement ref_count
 * invoked by vop_ref_dec
 * calls vop_reclaim if the ref_count hits zero
 * */
int inode_ref_dec(struct inode *node)
{
    assert(inode_ref_count(node) > 0);
    int ref_count, ret;
    
    node->ref_count -= 1;
    ref_count = node->ref_count;
    
    // 引用计数为 0，文件节点资源回收（文件真正删除），同时把内存节点最新数据同步到磁盘上
    if (ref_count == 0)
    {
        assert(node != NULL && node->in_ops != NULL && node->in_ops->vop_reclaim != NULL);
        inode_check(node, "reclaim");
        
        if ((ret = node->in_ops->vop_reclaim(node)) != 0 && ret != -E_BUSY)
        {
            cprintf("vfs: warning: vop_reclaim: %e.\n", ret);
        }
    }
    return ref_count;
}

/* *
 * inode_open_inc - increment the open_count
 * invoked by vop_open_inc
 * */
int inode_open_inc(struct inode *node)
{
    node->open_count += 1;
    return node->open_count;
}

/* *
 * inode_open_dec - decrement the open_count
 * invoked by vop_open_dec
 * calls vop_close if the open_count hits zero
 * */
int inode_open_dec(struct inode *node)
{
    assert(inode_open_count(node) > 0);
    int open_count, ret;
    node->open_count -= 1;
    open_count = node->open_count;
    
    // 节点没有进程打开时，可以关闭节点
    if (open_count == 0)
    {
        assert(node != NULL && node->in_ops != NULL && node->in_ops->vop_close != NULL);
        inode_check(node, "close");

        if ((ret = node->in_ops->vop_close(node)) != 0)
        {
            cprintf("vfs: warning: vop_close: %e.\n", ret);
        }
    }
    return open_count;
}

/* *
 * inode_check - check the various things being valid
 * called before all vop_* calls
 * */
void inode_check(struct inode *node, const char *opstr)
{
    assert(node != NULL && node->in_ops != NULL);
    assert(node->in_ops->vop_magic == VOP_MAGIC);
    int ref_count = inode_ref_count(node);
    int open_count = inode_open_count(node);
    assert(ref_count >= open_count && open_count >= 0);
    assert(ref_count < MAX_INODE_COUNT && open_count < MAX_INODE_COUNT);
}

struct device *device_vop_info(struct inode *node)
{
    struct inode *__node = node;
    assert(__node != NULL && (__node->in_type == inode_type_device_info));
    return &(__node->in_info.__device_info);
}

struct sfs_inode *sfs_vop_info(struct inode *node)
{
    struct inode *__node = node;
    assert(__node != NULL && (__node->in_type == inode_type_sfs_inode_info));
    return &(__node->in_info.__sfs_inode_info);
}

struct pipe_inode *pipe_inode_vop_info(struct inode *node)
{
    struct inode *__node = node;
    assert(__node != NULL && (__node->in_type == inode_type_pipe_inode_info));
    return &(__node->in_info.__pipe_inode_info);
}

struct pipe_root *pipe_root_vop_info(struct inode *node)
{
    struct inode *__node = node;
    assert(__node != NULL && (__node->in_type == inode_type_pipe_root_info));
    return &(__node->in_info.__pipe_root_info);
}

struct ffs_inode *ffs_vop_info(struct inode *node)
{
    struct inode *__node = node;
    assert(__node != NULL && (__node->in_type == inode_type_ffs_inode_info));
    return &(__node->in_info.__ffs_inode_info);
}

/* *
 * null_vop_* - null vop functions
 * */
int null_vop_pass(void)
{
    return 0;
}

int null_vop_inval(void)
{
    return -E_INVAL;
}

int null_vop_unimp(void)
{
    return -E_UNIMP;
}

int null_vop_isdir(void)
{
    return -E_ISDIR;
}

int null_vop_notdir(void)
{
    return -E_NOTDIR;
}


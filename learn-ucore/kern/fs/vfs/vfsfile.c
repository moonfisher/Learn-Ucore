#include "defs.h"
#include "string.h"
#include "vfs.h"
#include "inode.h"
#include "unistd.h"
#include "error.h"
#include "assert.h"

int sfs_create(struct inode *node, const char *name, bool excl, struct inode **node_store);

// open file in vfs, get/create inode for file with filename path.
int vfs_open(char *path, uint32_t open_flags, struct inode **node_store)
{
    bool can_write = 0;
    switch (open_flags & O_ACCMODE)
    {
        case O_RDONLY:
            break;
        case O_WRONLY:
        case O_RDWR:
            can_write = 1;
            break;
        default:
            return -E_INVAL;
    }

    if (open_flags & O_TRUNC)
    {
        if (!can_write)
        {
            return -E_INVAL;
        }
    }

    int ret; 
    struct inode *node;
    bool excl = (open_flags & O_EXCL) != 0;
    bool create = (open_flags & O_CREAT) != 0;
    
    ret = vfs_lookup(path, &node);
    if (ret != 0)
    {
        // 文件路径不存在，看是否需要创建文件
        if (ret == -E_NOENT && (create))
        {
            char *name;
            struct inode *dir;
            if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0)
            {
                return ret;
            }
            
            assert(dir != NULL && dir->in_ops != NULL && dir->in_ops->vop_create != NULL);
            inode_check(dir, "create");
//            ret = dir->in_ops->vop_create(dir, name, excl, &node);
            ret = sfs_create(dir, name, excl, &node);
        }
        else
            return ret;
    }
    else if (excl && create)
    {
        return -E_EXISTS;
    }
    assert(node != NULL);
    
    assert(node != NULL && node->in_ops != NULL && node->in_ops->vop_open != NULL);
    inode_check(node, "open");
    if ((ret = node->in_ops->vop_open(node, open_flags)) != 0)
    {
        inode_ref_dec(node);
        return ret;
    }

    inode_open_inc(node);
    if (open_flags & O_TRUNC || create)
    {
        assert(node != NULL && node->in_ops != NULL && node->in_ops->vop_truncate != NULL);
        inode_check(node, "truncate");
   
        // 则将其长度截短为 0
        if ((ret = node->in_ops->vop_truncate(node, 0)) != 0)
        {
            inode_open_dec(node);
            inode_ref_dec(node);
            return ret;
        }
    }
    *node_store = node;
    return 0;
}

// close file in vfs
int vfs_close(struct inode *node)
{
    inode_open_dec(node);
    inode_ref_dec(node);
    return 0;
}

// unimplement
int vfs_unlink(char *path)
{
    int ret;
    char *name;
    struct inode *dir;
    if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0)
    {
        return ret;
    }
    
    assert(dir != NULL && dir->in_ops != NULL && dir->in_ops->vop_unlink != NULL);
    inode_check(dir, "unlink");
    ret = dir->in_ops->vop_unlink(dir, name);
    inode_ref_dec(dir);
    return ret;
}

// unimplement
int vfs_rename(char *old_path, char *new_path)
{
    int ret;
    char *old_name, *new_name;
    struct inode *old_dir, *new_dir;
    if ((ret = vfs_lookup_parent(old_path, &old_dir, &old_name)) != 0)
    {
        return ret;
    }
    if ((ret = vfs_lookup_parent(new_path, &new_dir, &new_name)) != 0)
    {
        inode_ref_dec(old_dir);
        return ret;
    }
    
    if (old_dir->in_fs == NULL || old_dir->in_fs != new_dir->in_fs)
    {
        ret = -E_XDEV;
    }
    else
    {
        assert(old_dir != NULL && old_dir->in_ops != NULL && old_dir->in_ops->vop_rename != NULL);
        inode_check(old_dir, "rename");
        ret = old_dir->in_ops->vop_rename(old_dir, old_name, new_dir, new_name);
    }
    inode_ref_dec(old_dir);
    inode_ref_dec(new_dir);
    return ret;
}

// unimplement
int vfs_link(char *old_path, char *new_path)
{
    int ret;
    char *new_name;
    struct inode *old_node, *new_dir;
    if ((ret = vfs_lookup(old_path, &old_node)) != 0)
    {
        return ret;
    }
    if ((ret = vfs_lookup_parent(new_path, &new_dir, &new_name)) != 0)
    {
        inode_ref_dec(old_node);
        return ret;
    }
    
    if (old_node->in_fs == NULL || old_node->in_fs != new_dir->in_fs)
    {
        ret = -E_XDEV;
    }
    else
    {
        assert(new_dir != NULL && new_dir->in_ops != NULL && new_dir->in_ops->vop_link != NULL);
        inode_check(new_dir, "link");
        ret = new_dir->in_ops->vop_link(new_dir, new_name, old_node);
    }
    inode_ref_dec(old_node);
    inode_ref_dec(new_dir);
    return ret;
}

// unimplement
int vfs_symlink(char *old_path, char *new_path)
{
    int ret;
    char *new_name;
    struct inode *new_dir;
    if ((ret = vfs_lookup_parent(new_path, &new_dir, &new_name)) != 0)
    {
        return ret;
    }
    
    assert(new_dir != NULL && new_dir->in_ops != NULL && new_dir->in_ops->vop_symlink != NULL);
    inode_check(new_dir, "symlink");
    ret = new_dir->in_ops->vop_symlink(new_dir, new_name, old_path);
    inode_ref_dec(new_dir);
    return ret;
}

// unimplement
int vfs_readlink(char *path, struct iobuf *iob)
{
    int ret;
    struct inode *node;
    if ((ret = vfs_lookup(path, &node)) != 0)
    {
        return ret;
    }
    
    assert(node != NULL && node->in_ops != NULL && node->in_ops->vop_readlink != NULL);
    inode_check(node, "readlink");
    ret = node->in_ops->vop_readlink(node, iob);
    inode_ref_dec(node);
    return ret;
}

// unimplement
int vfs_mkdir(char *path)
{
    int ret;
    char *name;
    struct inode *dir;
    if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0)
    {
        return ret;
    }
    
    assert(dir != NULL && dir->in_ops != NULL && dir->in_ops->vop_mkdir != NULL);
    inode_check(dir, "mkdir");
    ret = dir->in_ops->vop_mkdir(dir, path);
    inode_ref_dec(dir);
    return ret;
}


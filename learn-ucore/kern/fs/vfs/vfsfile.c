#include "defs.h"
#include "string.h"
#include "vfs.h"
#include "inode.h"
#include "unistd.h"
#include "error.h"
#include "assert.h"


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
        if (ret == -16 && (create))
        {
            char *name;
            struct inode *dir;
            if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0)
            {
                return ret;
            }
            
            assert(dir != NULL && dir->in_ops != NULL && dir->in_ops->vop_create != NULL);
            inode_check(dir, "create");
            ret = dir->in_ops->vop_create(dir, name, excl, &node);
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
    return -E_UNIMP;
}

// unimplement
int vfs_rename(char *old_path, char *new_path)
{
    return -E_UNIMP;
}

// unimplement
int vfs_link(char *old_path, char *new_path)
{
    return -E_UNIMP;
}

// unimplement
int vfs_symlink(char *old_path, char *new_path)
{
    return -E_UNIMP;
}

// unimplement
int vfs_readlink(char *path, struct iobuf *iob)
{
    return -E_UNIMP;
}

// unimplement
int vfs_mkdir(char *path)
{
    return -E_UNIMP;
}

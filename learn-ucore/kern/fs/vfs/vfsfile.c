#include "defs.h"
#include "string.h"
#include "vfs.h"
#include "inode.h"
#include "unistd.h"
#include "error.h"
#include "assert.h"

int sfs_create(struct inode *node, const char *name, bool excl, struct inode **node_store);
int pipe_root_create(struct inode *__node, const char *name, bool excl, struct inode **node_store);
int ffs_create(struct inode *node, const char *name, bool excl, struct inode **node_store);

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
    struct inode *dir = NULL, *node = NULL;
    if (open_flags & O_CREAT)
    {
        char *name;
        bool excl = (open_flags & O_EXCL) != 0;
        if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0)
        {
            return ret;
        }
        
        assert(dir != NULL && dir->in_ops != NULL && dir->in_ops->vop_create != NULL);
        inode_check(dir, "create");
        //            ret = dir->in_ops->vop_create(dir, name, excl, &node);
        if (dir->in_fs->fs_type == fs_type_pipe_info)
        {
            ret = pipe_root_create(dir, name, excl, &node);
        }
        else if (dir->in_fs->fs_type == fs_type_sfs_info)
        {
            ret = sfs_create(dir, name, excl, &node);
        }
        else if (dir->in_fs->fs_type == fs_type_ffs_info)
        {
            ret = ffs_create(dir, name, excl, &node);
        }
        inode_ref_dec(dir);
    }
    else
    {
        ret = vfs_lookup(path, &node);
    }
    
    if (ret != 0)
    {
        return ret;
    }
    
    assert(node != NULL && node->in_ops != NULL && node->in_ops->vop_open != NULL);
    inode_check(node, "open");
    if ((ret = node->in_ops->vop_open(node, open_flags)) != 0)
    {
        inode_ref_dec(node);
        return ret;
    }

    inode_open_inc(node);
    if (open_flags & O_TRUNC)
    {
        assert(node != NULL && node->in_ops != NULL && node->in_ops->vop_truncate != NULL);
        inode_check(node, "truncate");
   
        // O_TRUNC 如果此文件存在，而且为只读或读写成功打开，则将其长度截短为 0。
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

/*
 创建硬链接，所谓硬链接，就是新旧文件的目录项不同，但指向相同的文件实体
 link 之后的新旧文件，sfs_disk_entry 结构不同，但指向相同的 sfs_disk_inode 节点
 
 硬链接的作用：
    1.节省硬盘空间。同样的文件，只需要维护硬连接关系，不需要进行多重的拷贝，这样可以节省硬盘空间。
    2.重命名文件。重命名文件并不需要打开该文件，只需改动某个目录项的内容即可。
    3.删除文件。删除文件只需将相应的目录项删除，该文件的链接数减 1, 如果删除目录项后该文件的链接数为零，
      这时系统才把真正的文件从磁盘上删除。
    4.文件更新。如果涉及文件更新，只需要先下载好一个新版本，然后修改面同名文件的硬即可改变,
      通过创建链接节点，极大的提高了工作开发的效率
 
 尽管硬链接节省空间，也是操作系统整合文件系统的传统方式，但是存在一些不足之处：
    1. 不允许给目录创建硬链接。
    2. 不可以在不同文件系统的文件间建立链接。因为 inode 是这个文件在当前分区中的索引值，
       是相对于这个分区的，当然不能跨越文件系统了。
*/
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
    
    // 不可以在不同文件系统的文件间建立链接。
    // 因为 inode 是这个文件在当前分区中的索引值，是相对于这个分区的，当然不能跨越文件系统了
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

/*
 软连接(symbolic link)又叫符号连接。符号连接相当于 Windows 下的快捷方式。
 软链接实际上只是一段文字，里面包含着它所指向的文件的名字，系统看到软链接后自动跳到对应的文件位置处进行处理
*/
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


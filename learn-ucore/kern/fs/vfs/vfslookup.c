#include "defs.h"
#include "string.h"
#include "vfs.h"
#include "inode.h"
#include "error.h"
#include "assert.h"

int sfs_lookup(struct inode *node, char *path, struct inode **node_store);
int sfs_lookup_parent(struct inode *node, char *path, struct inode **node_store, char **endp);
int pipe_root_lookup(struct inode *node, char *path, struct inode **node_store);
int pipe_root_lookup_parent(struct inode *node, char *path, struct inode **node_store, char **endp);
/*
 * get_device- Common code to pull the device name, if any, off the front of a
 *             path and choose the inode to begin the name lookup relative to.
 */
// 根据文件名获取对应的 inode
static int get_device(char *path, char *subpath, struct inode **node_store)
{
    int i, slash = -1, colon = -1;
    for (i = 0; path[i] != '\0'; i ++)
    {
        if (path[i] == ':') { colon = i; break; }
        if (path[i] == '/') { slash = i; break; }
    }
    
    if (colon < 0 && slash != 0)
    {
        /* *
         * No colon before a slash, so no device name specified, and the slash isn't leading
         * or is also absent, so this is a relative path or just a bare filename. Start from
         * the current directory, and use the whole thing as the subpath.
         * */
        /*
         没有冒号，且斜杠也不是第一个，说明是一个相对路径或只是一个裸文件名，比如 sh 或者 test/test1
         从当前目录开始搜索，并使用整个作为子路径。
        */
        strcpy(subpath, path);
        return vfs_get_curdir(node_store);
    }
    
    if (colon > 0)
    {
        // 有冒号，且冒号不是第一个，说明是一个带根目录的访问方式，比如 disk0: 或者 disk0:/test，
        // 需要先找到对应的根目录
        /* device:path - get root of device's filesystem */
        char pathT[1024] = {0};
        strcpy(pathT, path);
        pathT[colon] = '\0';

        /* device:/path - skip slash, treat as device:path */
        while (pathT[++ colon] == '/');
        strcpy(subpath, pathT + colon);
        
        // 由于初始化时已将 disk0 的 vfs_dev_t 结构添加到 vdev_list 中，这里遍历链表即可找到对应的 inode
        return vfs_get_root(pathT, node_store);
    }

    /* *
     * we have either /path or :path
     * /path is a path relative to the root of the "boot filesystem"
     * :path is a path relative to the root of the current filesystem
     * */
    int ret;
    if (*path == '/')
    {
        // 类似 "/dir/test" 这种访问方式，需要先找到根目录节点
        if ((ret = vfs_get_bootfs(node_store)) != 0)
        {
            return ret;
        }
    }
    else
    {
        assert(*path == ':');
        struct inode *node;
        if ((ret = vfs_get_curdir(&node)) != 0)
        {
            return ret;
        }
        /* The current directory may not be a device, so it must have a fs. */
        assert(node->in_fs != NULL);
        *node_store = node->in_fs->fs_get_root(node->in_fs);
        inode_ref_dec(node);
    }

    /* ///... or :/... */
    while (*(++ path) == '/');
    strcpy(subpath, path);
    return 0;
}

/*
 * vfs_lookup - get the inode according to the path filename
 */
int vfs_lookup(char *path, struct inode **node_store)
{
    int ret;
    struct inode *node;
    char subpath[256] = {0};
    
    // 先根据 path 找到当前目录，可能是根目录，也可能是当前进程所在目录
    if ((ret = get_device(path, subpath, &node)) != 0)
    {
        return ret;
    }
    
    if (*subpath != '\0')
    {
        // 找到当前目录之后，再继续搜索后续路径，搜索子目录或者文件
        assert(node != NULL && node->in_ops != NULL && node->in_ops->vop_lookup != NULL);
        inode_check(node, "lookup");
//        ret = node->in_ops->vop_lookup(node, path, node_store);
        if (node->in_fs->fs_type == fs_type_sfs_info)
        {
            ret = sfs_lookup(node, subpath, node_store);
        }
        else if (node->in_fs->fs_type == fs_type_pipe_info)
        {
            ret = pipe_root_lookup(node, subpath, node_store);
        }
        inode_ref_dec(node);
        return ret;
    }
    
    *node_store = node;
    return 0;
}

/*
 * vfs_lookup_parent - Name-to-vnode translation.
 *  (In BSD, both of these are subsumed by namei().)
 */
int vfs_lookup_parent(char *path, struct inode **node_store, char **endp)
{
    int ret;
    struct inode *node;
    char subpath[256] = {0};
    
    if ((ret = get_device(path, subpath, &node)) != 0)
    {
        return ret;
    }

    if (*path == '\0')
    {
        return -E_INVAL;
    }
    
    if (node->in_fs->fs_type == fs_type_pipe_info)
    {
        ret = pipe_root_lookup_parent(node, path, node_store, endp);
        inode_ref_dec(node);
    }
    else if (node->in_fs->fs_type == fs_type_sfs_info)
    {
        ret = sfs_lookup_parent(node, path, node_store, endp);
        inode_ref_dec(node);
    }
    
    return 0;
}

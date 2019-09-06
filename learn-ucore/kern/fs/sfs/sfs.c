#include "defs.h"
#include "sfs.h"
#include "error.h"
#include "assert.h"
#include "vfs.h"

/*
 * sfs_init - mount sfs on disk0
 *
 * CALL GRAPH:
 *   kern_init-->fs_init-->sfs_init
 */
// 挂载 disk0，并安装 sfs 文件系统
void sfs_init(void)
{
    int ret;
    
    if ((ret = register_filesystem("sfs", sfs_mount)) != 0)
    {
        panic("failed: sfs: register_filesystem: %e.\n", ret);
    }

    // disk0 是缺省挂载的，后续 init 进程初始化会用 disk0 作为根目录
    if ((ret = sfs_mount("disk0", "", NULL)) != 0)
    {
        panic("failed: sfs: disk0 sfs_mount: %e.\n", ret);
    }
}


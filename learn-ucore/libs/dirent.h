#ifndef __LIBS_DIRENT_H__
#define __LIBS_DIRENT_H__

#include "defs.h"
#include "unistd.h"

struct dirent
{
    // 在目录文件中的偏移 
    off_t offset;
    // 文件名
    char name[FS_MAX_FNAME_LEN + 1];
};

#endif /* !__LIBS_DIRENT_H__ */


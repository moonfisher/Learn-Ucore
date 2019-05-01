#ifndef __LIBS_STAT_H__
#define __LIBS_STAT_H__

#include "defs.h"

struct stat
{
    // 文件保护模式
    uint32_t st_mode;                   // protection mode and file type
    // 硬连接数
    size_t st_nlinks;                   // number of hard links
    // 分配给文件的块的数量，512 字节为单元
    size_t st_blocks;                   // number of blocks file is using
    // 总大小，字节为单位
    size_t st_size;                     // file size (bytes)
};

#define S_IFMT          070000          // mask for type of file 文件类型
#define S_IFREG         010000          // ordinary regular file 文件是一个普通文件
#define S_IFDIR         020000          // directory 文件是一个目录
#define S_IFLNK         030000          // symbolic link 文件是一个符号链接
#define S_IFCHR         040000          // character device 文件是一个特殊的字符设备
#define S_IFBLK         050000          // block device 文件是一个特殊的块设备

#define S_ISREG(mode)                   (((mode) & S_IFMT) == S_IFREG)      // regular file
#define S_ISDIR(mode)                   (((mode) & S_IFMT) == S_IFDIR)      // directory
#define S_ISLNK(mode)                   (((mode) & S_IFMT) == S_IFLNK)      // symlink
#define S_ISCHR(mode)                   (((mode) & S_IFMT) == S_IFCHR)      // char device
#define S_ISBLK(mode)                   (((mode) & S_IFMT) == S_IFBLK)      // block device

#endif /* !__LIBS_STAT_H__ */


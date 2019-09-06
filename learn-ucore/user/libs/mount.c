#include "mount.h"
#include "syscall.h"

/*
 source：将要挂上的文件系统
 target：文件系统所要挂在的目标目录
 filesystemtype：文件系统的类型
 data：文件系统特有的参数
*/
// mount -t sfs test.img /mnt
int mount(const char *source, const char *target, const char *filesystemtype, const void *data)
{
	return sys_mount(source, target, filesystemtype, data);
}

// umount /mnt
int umount(const char *target)
{
	return sys_umount(target);
}

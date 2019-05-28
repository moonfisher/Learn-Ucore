#include "defs.h"
#include "unistd.h"
#include "stdarg.h"
#include "syscall.h"
#include "stat.h"
#include "dirent.h"

#define MAX_ARGS            5

// 通过陷阱门来实现系统调用
static inline int syscall(int num, ...)
{
    va_list ap;
    va_start(ap, num);
    uint32_t a[MAX_ARGS];
    int i, ret;
    for (i = 0; i < MAX_ARGS; i ++)
    {
        a[i] = va_arg(ap, uint32_t);
    }
    va_end(ap);

    asm volatile (
        "int %1;"
        : "=a" (ret)
        : "i" (T_SYSCALL),
          "a" (num),
          "d" (a[0]),
          "c" (a[1]),
          "b" (a[2]),
          "D" (a[3]),
          "S" (a[4])
        : "cc", "memory");
    return ret;
}

// 通过调用门来实现系统调用
//static inline int syscall(int num, ...)
//{
//    va_list ap;
//    va_start(ap, num);
//    uint32_t a[MAX_ARGS];
//    int i, ret;
//    for (i = 0; i < MAX_ARGS; i ++)
//    {
//        a[i] = va_arg(ap, uint32_t);
//    }
//    va_end(ap);
//
//    asm volatile ("mov %0, %%eax" :: "r" (num) : "memory");
//    asm volatile ("mov %0, %%edx" :: "r" (a[0]) : "memory");
//    asm volatile ("mov %0, %%ecx" :: "r" (a[1]) : "memory");
//    asm volatile ("mov %0, %%ebx" :: "r" (a[2]) : "memory");
//    asm volatile ("mov %0, %%edi" :: "r" (a[3]) : "memory");
//    asm volatile ("mov %0, %%esi" :: "r" (a[4]) : "memory");
//    asm volatile ("call $0x30, $0;");
//    asm volatile ("mov %%eax, %0" : "=r" (ret) :: "memory");
////    asm volatile (
////                  "call $0x30, $0;"
////                  : "=a" (ret)
////                  : "a" (num),
////                  "d" (a[0]),
////                  "c" (a[1]),
////                  "b" (a[2]),
////                  "D" (a[3]),
////                  "S" (a[4])
////                  : "cc", "memory");
//    return ret;
//}

int sys_exit(int error_code)
{
    return syscall(SYS_exit, error_code);
}

int sys_fork(char *name)
{
    return syscall(SYS_fork, name);
}

int sys_wait(int pid, int *store)
{
    return syscall(SYS_wait, pid, store);
}

int sys_yield(void)
{
    return syscall(SYS_yield);
}

int sys_kill(int pid)
{
    return syscall(SYS_kill, pid);
}

int sys_getpid(void)
{
    return syscall(SYS_getpid);
}

int sys_putc(int c)
{
    return syscall(SYS_putc, c);
}

int sys_pgdir(void)
{
    return syscall(SYS_pgdir);
}

void sys_set_priority(uint32_t priority)
{
    syscall(SYS_set_priority, priority);
}

int sys_sleep(unsigned int time)
{
    return syscall(SYS_sleep, time);
}

size_t sys_gettime(void)
{
    return syscall(SYS_gettime);
}

int sys_brk(uintptr_t * brk_store)
{
    return syscall(SYS_brk, brk_store);
}

int sys_mmap(uintptr_t * addr_store, size_t len, uint32_t mmap_flags)
{
    return syscall(SYS_mmap, addr_store, len, mmap_flags);
}

int sys_munmap(uintptr_t addr, size_t len)
{
    return syscall(SYS_munmap, addr, len);
}

int sys_exec(const char *name, int argc, const char **argv)
{
    return syscall(SYS_exec, name, argc, argv);
}

int sys_open(const char *path, uint32_t open_flags)
{
    return syscall(SYS_open, path, open_flags);
}

int sys_close(int fd)
{
    return syscall(SYS_close, fd);
}

int sys_read(int fd, void *base, size_t len)
{
    return syscall(SYS_read, fd, base, len);
}

int sys_write(int fd, void *base, size_t len)
{
    return syscall(SYS_write, fd, base, len);
}

int sys_seek(int fd, off_t pos, int whence)
{
    return syscall(SYS_seek, fd, pos, whence);
}

int sys_fstat(int fd, struct stat *stat)
{
    return syscall(SYS_fstat, fd, stat);
}

int sys_fsync(int fd)
{
    return syscall(SYS_fsync, fd);
}

int sys_getcwd(char *buffer, size_t len)
{
    return syscall(SYS_getcwd, buffer, len);
}

int sys_getdirentry(int fd, struct dirent *dirent)
{
    return syscall(SYS_getdirentry, fd, dirent);
}

int sys_dup(int fd1, int fd2)
{
    return syscall(SYS_dup, fd1, fd2);
}

int sys_mkdir(const char *path)
{
    return syscall(SYS_mkdir, path);
}

int sys_rm(const char *path)
{
    return syscall(SYS_mkdir, path);
}

int sys_chdir(const char *path)
{
    return syscall(SYS_chdir, path);
}

int sys_rename(const char *path1, const char *path2)
{
    return syscall(SYS_rename, path1, path2);
}

int sys_pipe(int *fd_store)
{
    return syscall(SYS_pipe, fd_store);
}

int sys_mkfifo(const char *name, uint32_t open_flags)
{
    return syscall(SYS_mkfifo, name, open_flags);
}

int sys_link(const char *path1, const char *path2)
{
    return syscall(SYS_link, path1, path2);
}

int sys_unlink(const char *path)
{
    return syscall(SYS_unlink, path);
}

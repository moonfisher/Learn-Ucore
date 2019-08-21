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
//    asm volatile (
//                  "call $0x30, $0;"
//                  : "=a" (ret)
//                  : "a" (num),
//                  "d" (a[0]),
//                  "c" (a[1]),
//                  "b" (a[2]),
//                  "D" (a[3]),
//                  "S" (a[4])
//                  : "cc", "memory");
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

int sys_shmem(uintptr_t *addr_store, size_t len, uint32_t mmap_flags)
{
    return syscall(SYS_shmem, (int)addr_store, len, mmap_flags, 0, 0);
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

int sys_open(const char *path, uint32_t open_flags, uint32_t arg2)
{
    return syscall(SYS_open, path, open_flags, arg2);
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

int sys_mount(const char *source, const char *target, const char *filesystemtype, const void *data)
{
    return syscall(SYS_mount, source, target, filesystemtype, data);
}

int sys_umount(const char *target)
{
    return syscall(SYS_umount, target);
}

int sys_sigaction(int sign, struct sigaction *act, struct sigaction *old)
{
    return syscall(SYS_sigaction, sign, act, old);
}

int sys_sigtkill(int pid, int sign)
{
    return syscall(SYS_tkill, pid, sign);
}

int sys_sigprocmask(int how, const sigset_t *set, sigset_t *old)
{
    return syscall(SYS_sigprocmask, how, set, old);
}

int sys_sigsuspend(uint32_t mask)
{
    return syscall(SYS_sigsuspend, mask);
}

int sys_sigreturn(uint32_t mask)
{
    return syscall(SYS_sigreturn, mask);
}

int sys_receive_packet(uint8_t *buf, size_t len, size_t* len_store)
{
    return syscall(SYS_receive_packet, (int)buf, len, (int)len_store, 0, 0 );
}

int sys_transmit_packet(uint8_t *buf, size_t len,size_t* len_store)
{
    return syscall(SYS_transmit_packet, (int)buf, len, (int)len_store, 0, 0);
}

int sys_send_event(int pid, int event_type, int event)
{
    return syscall(SYS_event_send, pid, event_type, event, 0, 0);
}

int sys_recv_event(int *pid_store, int event_type, int *event_store, unsigned int timeout)
{
    return syscall(SYS_event_recv, (int)pid_store, (int)event_type, (int)event_store, timeout, 0, 0);
}

int sys_mbox_init(unsigned int max_slots)
{
    return syscall(SYS_mbox_init, max_slots, 0, 0, 0, 0);
}

int sys_mbox_send(int id, struct mboxbuf *buf, unsigned int timeout)
{
    return syscall(SYS_mbox_send, id, (int)buf, timeout, 0, 0);
}

int sys_mbox_recv(int id, struct mboxbuf *buf, unsigned int timeout)
{
    return syscall(SYS_mbox_recv, id, (int)buf, timeout, 0, 0);
}

int sys_mbox_free(int id)
{
    return syscall(SYS_mbox_free, id, 0, 0, 0, 0);
}

int sys_mbox_info(int id, struct mboxinfo *info)
{
    return syscall(SYS_mbox_info, id, (int)info, 0, 0, 0);
}

int sys_ping(char *target, int len)
{
   return syscall(SYS_ping, (int)target, len, 0);
}

int sys_process_dump()
{
    return syscall(SYS_process_dump, 0, 0, 0);
}

int sys_rtdump()
{
    return syscall(SYS_rtdump, 0, 0, 0);
}

int sys_arpprint()
{
    return syscall(SYS_arpprint, 0, 0, 0);
}

int sys_netstatus()
{
    return syscall(SYS_netstatus, 0, 0, 0);
}

int sys_sock_socket(uint32_t type, const char* ipaddr, uint32_t iplen)
{
    return syscall(SYS_sock_socket, type, (int)ipaddr , iplen, 0, 0);
}

int sys_sock_listen(uint32_t tcpfd, uint32_t qsize)
{
    return syscall(SYS_sock_listen, tcpfd, qsize, 0);
}

int sys_sock_accept(uint32_t listenfd, uint32_t timeout)
{
    return syscall(SYS_sock_accept, listenfd, timeout, 0);
}

int sys_sock_connect(uint32_t sockfd, const char* ipaddr, uint32_t iplen)
{
    return syscall(SYS_sock_connect, sockfd, (int)ipaddr, iplen, 0, 0);
}

int sys_sock_bind(uint32_t sockfd, uint32_t lport, uint32_t rport)
{
    return syscall(SYS_sock_bind, sockfd, lport, rport, 0, 0);
}

int sys_sock_send(uint32_t sockfd, char* buf, uint32_t len, uint32_t timeout)
{
    return syscall(SYS_sock_send, sockfd, (int)buf, len, timeout, 0);
}

int sys_sock_recv(uint32_t sockfd, char* buf, uint32_t len, uint32_t timeout)
{
    return syscall(SYS_sock_recv, sockfd, (int)buf, len, timeout, 0);
}

int sys_sock_close(uint32_t sockfd)
{
    return syscall(SYS_sock_close, sockfd, 0, 0);
}

int sys_sock_shutdown(uint32_t sockfd, uint32_t type)
{
    return syscall(SYS_sock_shutdown, sockfd, type, 0);
}






#include "defs.h"
#include "unistd.h"
#include "proc.h"
#include "syscall.h"
#include "trap.h"
#include "stdio.h"
#include "pmm.h"
#include "assert.h"
#include "clock.h"
#include "stat.h"
#include "dirent.h"
#include "sysfile.h"
#include "mbox.h"
#include "slab.h"
#include "e1000/e1000.h"
#include "tcpip/h/network.h"
#include "event.h"
#include "vfs.h"

extern void netstatus();

static int sys_exit(uint32_t arg[])
{
    int error_code = (int)arg[0];
    return do_exit(error_code);
}

static int sys_fork(uint32_t arg[])
{
    struct trapframe *tf = current->tf;
    uintptr_t stack = tf->tf_esp;
    const char *name = (const char *)arg[0];
    return do_fork(0, stack, tf, name);
}

static int sys_wait(uint32_t arg[])
{
    int pid = (int)arg[0];
    int *store = (int *)arg[1];
    return do_wait(pid, store);
}

static int sys_exec(uint32_t arg[])
{
    const char *name = (const char *)arg[0];
    int argc = (int)arg[1];
    const char **argv = (const char **)arg[2];
    return do_execve(name, argc, argv);
}

static int sys_clone(uint32_t arg[])
{
    struct trapframe *tf = current->tf;
    uint32_t clone_flags = (uint32_t) arg[0];
    uintptr_t stack = (uintptr_t) arg[1];
    char *name = (char *)arg[3];
    if (stack == 0)
    {
        // 如果没有为子进程或者线程分配新的用户栈空间，则使用父进程的栈空间
        stack = tf->tf_esp;
    }
    return do_fork(clone_flags, stack, tf, name);
}

static int sys_exit_thread(uint32_t arg[])
{
    int error_code = (int)arg[0];
    return do_exit_thread(error_code);
}

static int sys_yield(uint32_t arg[])
{
    return do_yield();
}

static int sys_kill(uint32_t arg[])
{
    int pid = (int)arg[0];
    return do_kill(pid, -E_KILLED);
}

static int sys_getpid(uint32_t arg[])
{
    return current->pid;
}

static int sys_brk(uint32_t arg[])
{
    uintptr_t *brk_store = (uintptr_t *)arg[0];
    return do_brk(brk_store);
}

static int sys_mmap(uint32_t arg[])
{
    uintptr_t *addr_store = (uintptr_t *)arg[0];
    size_t len = (size_t) arg[1];
    uint32_t mmap_flags = (uint32_t) arg[2];
    return do_mmap(addr_store, len, mmap_flags);
}

static int sys_munmap(uint32_t arg[])
{
    uintptr_t addr = (uintptr_t)arg[0];
    size_t len = (size_t) arg[1];
    return do_munmap(addr, len);
}

static int sys_shmem(uint32_t arg[])
{
    uintptr_t *addr_store = (uintptr_t *)arg[0];
    size_t len = (size_t)arg[1];
    uint32_t mmap_flags = (uint32_t)arg[2];
    return do_shmem(addr_store, len, mmap_flags);
}

static int sys_putc(uint32_t arg[])
{
    int c = (int)arg[0];
    cputchar(c);
    return 0;
}

static int sys_pgdir(uint32_t arg[])
{
    print_pgdir();
    return 0;
}

static int sys_gettime(uint32_t arg[])
{
    return (int)ticks;
}

static int sys_set_priority(uint32_t arg[])
{
    uint32_t priority = (uint32_t)arg[0];
    set_priority(priority);
    return 0;
}

static int sys_sleep(uint32_t arg[])
{
    unsigned int time = (unsigned int)arg[0];
    return do_sleep(time);
}

static int sys_open(uint32_t arg[])
{
    const char *path = (const char *)arg[0];
    uint32_t open_flags = (uint32_t)arg[1];
    return sysfile_open(path, open_flags);
}

static int sys_close(uint32_t arg[])
{
    int fd = (int)arg[0];
    return sysfile_close(fd);
}

static int sys_read(uint32_t arg[])
{
    int fd = (int)arg[0];
    void *base = (void *)arg[1];
    size_t len = (size_t)arg[2];
    return sysfile_read(fd, base, len);
}

static int sys_write(uint32_t arg[])
{
    int fd = (int)arg[0];
    void *base = (void *)arg[1];
    size_t len = (size_t)arg[2];
    return sysfile_write(fd, base, len);
}

static int sys_seek(uint32_t arg[])
{
    int fd = (int)arg[0];
    off_t pos = (off_t)arg[1];
    int whence = (int)arg[2];
    return sysfile_seek(fd, pos, whence);
}

static int sys_fstat(uint32_t arg[])
{
    int fd = (int)arg[0];
    struct stat *stat = (struct stat *)arg[1];
    return sysfile_fstat(fd, stat);
}

static int sys_fsync(uint32_t arg[])
{
    int fd = (int)arg[0];
    return sysfile_fsync(fd);
}

static int sys_chdir(uint32_t arg[])
{
    const char *path = (const char *)arg[0];
    return sysfile_chdir(path);
}

static int sys_getcwd(uint32_t arg[])
{
    char *buf = (char *)arg[0];
    size_t len = (size_t)arg[1];
    return sysfile_getcwd(buf, len);
}

static int sys_mkdir(uint32_t arg[])
{
    const char *path = (const char *)arg[0];
    return sysfile_mkdir(path);
}

static int sys_getdirentry(uint32_t arg[])
{
    int fd = (int)arg[0];
    struct dirent *direntp = (struct dirent *)arg[1];
    return sysfile_getdirentry(fd, direntp);
}

static int sys_dup(uint32_t arg[])
{
    int fd1 = (int)arg[0];
    int fd2 = (int)arg[1];
    return sysfile_dup(fd1, fd2);
}

static int sys_rename(uint32_t arg[])
{
    const char *path1 = (const char *)arg[0];
    const char *path2 = (const char *)arg[1];
    return sysfile_rename(path1, path2);
}

static int sys_pipe(uint32_t arg[])
{
    int *fd_store = (int *)arg[0];
    return sysfile_pipe(fd_store);
}

static int sys_mkfifo(uint32_t arg[])
{
    const char *name = (const char *)arg[0];
    uint32_t open_flags = (uint32_t) arg[1];
    return sysfile_mkfifo(name, open_flags);
}

static int sys_link(uint32_t arg[])
{
    const char *path1 = (const char *)arg[0];
    const char *path2 = (const char *)arg[1];
    return sysfile_link(path1, path2);
}

static int sys_unlink(uint32_t arg[])
{
    const char *name = (const char *)arg[0];
    return sysfile_unlink(name);
}

static int sys_mount(uint32_t arg[])
{
    const char *source = (const char *)arg[0];
//    const char *target = (const char *)arg[1];
    const char *filesystemtype = (const char *)arg[2];
//    const void *data = (const void *)arg[3];
    return do_mount(source, filesystemtype);
}

static int sys_umount(uint32_t arg[])
{
    const char *target = (const char *)arg[0];
    return do_umount(target);
}

static int sys_sigaction(uint32_t arg[])
{
    return do_sigaction((int)arg[0], (const struct sigaction *)arg[1], (struct sigaction *)arg[2]);
}

static int sys_sigprocmask(uint32_t arg[])
{
    return do_sigprocmask((int)arg[0], (const sigset_t *)arg[1], (sigset_t *) arg[2]);
}

//static int sys_sigpending(uint32_t arg[])
//{
//    return do_sigpending((sigset_t *) arg[0]);
//}

static int sys_sigtkill(uint32_t arg[])
{
    return do_sigtkill((int)arg[0], (int)arg[1]);
}

static int sys_sigsuspend(uint32_t arg[])
{
    return do_sigsuspend((sigset_t *) arg[0]);
}

//static int sys_sigkill(uint32_t arg[])
//{
//    return do_sigkill((int)arg[0], (int)arg[1]);
//}

//static int sys_sigaltstack(uint32_t arg[])
//{
//    const stack_t *stack = (const stack_t *)arg[0];
//    stack_t *old = (stack_t *) arg[1];
//    return do_sigaltstack(stack, old);
//}

//static int sys_sigwaitinfo(uint32_t arg[])
//{
//    const sigset_t *set = (const sigset_t *)arg[0];
//    struct siginfo_t *info = (struct siginfo_t *)arg[1];
//    return do_sigwaitinfo(set, info);
//}

//this never used by user program
static int sys_sigreturn(uint32_t arg[])
{
    return do_sigreturn();
}

static int sys_event_send(uint32_t arg[])
{
    int pid = (int)arg[0];
    int event_type = (int)arg[1];
    int event = (int)arg[2];
    return ipc_event_send(pid, event_type, event);
}

static int sys_event_recv(uint32_t arg[])
{
    int *pid_store = (int *)arg[0];
    int event_type = (int)arg[1];
    int *event_store = (int *)arg[2];
    unsigned int timeout = (unsigned int)arg[3];
    return ipc_event_recv(pid_store, event_type, event_store, timeout);
}

static int sys_mbox_init(uint32_t arg[])
{
    unsigned int max_slots = (unsigned int)arg[0];
    return ipc_mbox_init(max_slots);
}

static int sys_mbox_send(uint32_t arg[])
{
    int id = (int)arg[0];
    struct mboxbuf *buf = (struct mboxbuf *)arg[1];
    unsigned int timeout = (unsigned int)arg[2];
    return ipc_mbox_send(id, buf, timeout);
}

static int sys_mbox_recv(uint32_t arg[])
{
    int id = (int)arg[0];
    struct mboxbuf *buf = (struct mboxbuf *)arg[1];
    unsigned int timeout = (unsigned int)arg[2];
    return ipc_mbox_recv(id, buf, timeout);
}

static int sys_mbox_free(uint32_t arg[])
{
    int id = (int)arg[0];
    return ipc_mbox_free(id);
}

static int sys_mbox_info(uint32_t arg[])
{
    int id = (int)arg[0];
    struct mboxinfo *info = (struct mboxinfo *)arg[1];
    return ipc_mbox_info(id, info);
}

static int sys_transmit_packet(uint32_t arg[])
{
    uintptr_t buf = (uintptr_t)arg[0];
    size_t len = arg[1];
    user_mem_check(current->mm, buf, len, 0);
    return e1000_transmit((uint8_t *)buf, (uint32_t)len);
}

// Receive a network packet.
//
// Return 0 on success, < 0 on error.
// Errors are:
//  -E_RX_QUEUE_EMPTY if receive queue is empty.
//  -E_BUF_TOO_SMALL if buffer is too small.
static int sys_receive_packet(uint32_t arg[])
{
    ssize_t r;
    uint8_t* buf = (uint8_t*)arg[0];
    size_t len = (size_t)arg[1];
    size_t* len_store = (size_t*) arg[2];
    cprintf("kernel mode sys_receive_packet buf addr =%x \n",buf);
    //user_mem_check(current->mm, buf, len, PTE_W);
    if (len_store != NULL)
    {
        user_mem_check(current->mm, (uintptr_t)buf, sizeof(size_t), PTE_W);
    }

    r = e1000_recv(buf, len);
    if (r < 0)
    {
        return r;
    }

    if (len_store != NULL)
    {
        *len_store = r;
    }
    return 0;
}

static int sys_ping(uint32_t arg[])
{
    char *buf = (char *)arg[0];
    size_t len = arg[1];
    assert(len > 0);
    char *tmpbuf = (char *)kmalloc(len+1);
    memcpy(tmpbuf, buf, len);
    tmpbuf[len] = 0;
    ping(tmpbuf, len);
    kfree(tmpbuf);
    return 0;
}

static int sys_process_dump(uint32_t arg[])
{
    process_dump();
    return 0;
}

static int sys_rtdump(uint32_t arg[])
{
    rtdump();
    return 0;
}

static int sys_arpprint(uint32_t arg[])
{
    arpprint();
    return 0;
}

static int sys_netstatus(uint32_t arg[])
{
    netstatus();
    return 0;
}

static int sys_sock_socket(uint32_t args[] /*uint32_t type, const char* ipaddr, uint32_t iplen*/)
{
//    uint32_t type = (uint32_t) args[0];
//    const char* ipaddr = (const char*) args[1];
//    uint32_t iplen = args[2];
//    int fd = -1;
//    //fd = so
//    //return syscall(SYS_socket, type, ipaddr , iplen, 0, 0);
    return -1;
}

static int sys_sock_listen(uint32_t arg[] /*uint32_t tcpfd, uint32_t qsize*/)
{
    //return syscall(SYS_listen, tcpfd, qsize);
    return -1;
}

static int sys_sock_accept(uint32_t arg[] /*uint32_t listenfd, uint32_t timeout*/)
{
    //return syscall(SYS_accept, listenfd, timeout, 0, 0, 0);
    return -1;
}

static int sys_sock_connect(/*uint32_t sockfd, const char* ipaddr, uint32_t iplen*/)
{
    //return syscall(SYS_connect, sockfd, ipaddr, ipaddr, 0, 0);
    return -1;
}

static int sys_sock_bind(uint32_t arg[] /*uint32_t sockfd, uint32_t lport, uint32_t rport*/)
{
    //return syscall(SYS_bind, sockfd, lport, rport, 0, 0);
    return -1;
}

static int sys_sock_send(uint32_t arg[] /*uint32_t sockfd, char* buf, uint32_t len, uint32_t timeout*/)
{
    //return syscall(SYS_send, sockfd, buf, len, timeout, 0);
    return -1;
}

static int sys_sock_recv(uint32_t arg[] /*uint32_t sockfd, char* buf, uint32_t len, uint32_t timeout*/)
{
    //return syscall(SYS_recv, sockfd, buf, len, timeout, 0);
    return -1;
}

static int sys_sock_close(uint32_t arg[] /*uint32_t sockfd*/)
{
    //return syscall(SYS_close_sock, sockfd, 0, 0, 0, 0);
    return -1;
}

static int sys_sock_shutdown(uint32_t arg[] /*uint32_t sockfd, uint32_t type*/)
{
    //return syscall(SYS_shutdown_scok, sockfd, type, 0, 0, 0);
    return -1;
}

static int (*syscalls[])(uint32_t arg[]) = {
    [SYS_exit]             = sys_exit,
    [SYS_fork]             = sys_fork,
    [SYS_wait]             = sys_wait,
    [SYS_exec]             = sys_exec,
    [SYS_yield]            = sys_yield,
    [SYS_sleep]            = sys_sleep,
    [SYS_kill]             = sys_kill,
    [SYS_getpid]           = sys_getpid,
    [SYS_brk]              = sys_brk,
    [SYS_mmap]             = sys_mmap,
    [SYS_munmap]           = sys_munmap,
    [SYS_shmem]            = sys_shmem,
    [SYS_putc]             = sys_putc,
    [SYS_pgdir]            = sys_pgdir,
    [SYS_gettime]          = sys_gettime,
    [SYS_clone]            = sys_clone,
    [SYS_exit_thread]      = sys_exit_thread,

    [SYS_event_send]       = sys_event_send,
    [SYS_event_recv]       = sys_event_recv,
    [SYS_mbox_init]        = sys_mbox_init,
    [SYS_mbox_send]        = sys_mbox_send,
    [SYS_mbox_recv]        = sys_mbox_recv,
    [SYS_mbox_free]        = sys_mbox_free,
    [SYS_mbox_info]        = sys_mbox_info,
    
    [SYS_set_priority]     = sys_set_priority,
    [SYS_open]             = sys_open,
    [SYS_close]            = sys_close,
    [SYS_read]             = sys_read,
    [SYS_write]            = sys_write,
    [SYS_seek]             = sys_seek,
    [SYS_fstat]            = sys_fstat,
    [SYS_fsync]            = sys_fsync,
    [SYS_getcwd]           = sys_getcwd,
    [SYS_mkdir]            = sys_mkdir,
    [SYS_getdirentry]      = sys_getdirentry,
    [SYS_dup]              = sys_dup,
    [SYS_chdir]            = sys_chdir,
    [SYS_link]             = sys_link,
    [SYS_rename]           = sys_rename,
    [SYS_unlink]           = sys_unlink,
    [SYS_pipe]             = sys_pipe,
    [SYS_mkfifo]           = sys_mkfifo,
    [SYS_mount]            = sys_mount,
    [SYS_umount]           = sys_umount,
    
//    [SYS_ioctl]            = sys_ioctl,
    [SYS_tkill]            = sys_sigtkill,
    [SYS_sigaction]        = sys_sigaction,
    [SYS_sigprocmask]      = sys_sigprocmask,
    [SYS_sigsuspend]       = sys_sigsuspend,
    [SYS_sigreturn]        = sys_sigreturn,
    
    [SYS_transmit_packet]  = sys_transmit_packet,
    [SYS_receive_packet]   = sys_receive_packet,
    [SYS_ping]             = sys_ping,
    [SYS_process_dump]     = sys_process_dump,
    [SYS_rtdump]           = sys_rtdump,
    [SYS_arpprint]         = sys_arpprint,
    [SYS_netstatus]        = sys_netstatus,

    [SYS_sock_socket]      = sys_sock_socket,
    [SYS_sock_listen]      = sys_sock_listen,
    [SYS_sock_accept]      = sys_sock_accept,
    [SYS_sock_connect]     = sys_sock_connect,
    [SYS_sock_bind]        = sys_sock_bind,
    [SYS_sock_send]        = sys_sock_send,
    [SYS_sock_recv]        = sys_sock_recv,
    [SYS_sock_close]       = sys_sock_close,
    [SYS_sock_shutdown]    = sys_sock_shutdown,
};

#define NUM_SYSCALLS        ((sizeof(syscalls)) / (sizeof(syscalls[0])))

void syscall(void)
{
    struct trapframe *tf = current->tf;
    uint32_t arg[5];
    int num = tf->tf_regs.reg_eax;
    if (num >= 0 && num < NUM_SYSCALLS)
    {
        if (syscalls[num] != NULL)
        {
            arg[0] = tf->tf_regs.reg_edx;
            arg[1] = tf->tf_regs.reg_ecx;
            arg[2] = tf->tf_regs.reg_ebx;
            arg[3] = tf->tf_regs.reg_edi;
            arg[4] = tf->tf_regs.reg_esi;
            tf->tf_regs.reg_eax = syscalls[num](arg);
            return ;
        }
    }
    print_trapframe(tf);
    panic("undefined syscall %d, pid = %d, name = %s.\n", num, current->pid, current->name);
}


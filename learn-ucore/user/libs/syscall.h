#ifndef __USER_LIBS_SYSCALL_H__
#define __USER_LIBS_SYSCALL_H__

#include "mboxbuf.h"
#include "signum.h"

struct stat;
struct dirent;

int sys_exit(int error_code);
int sys_fork(char *name);
int sys_wait(int pid, int *store);
int sys_exec(const char *name, int argc, const char **argv);
int sys_yield(void);
int sys_kill(int pid);
int sys_getpid(void);
int sys_brk(uintptr_t *brk_store);
int sys_mmap(uintptr_t *addr_store, size_t len, uint32_t mmap_flags);
int sys_munmap(uintptr_t addr, size_t len);
int sys_shmem(uintptr_t *addr_store, size_t len, uint32_t mmap_flags);
int sys_putc(int c);
int sys_pgdir(void);
int sys_sleep(unsigned int time);
size_t sys_gettime(void);

int sys_open(const char *path, uint32_t open_flags, uint32_t arg2);
int sys_close(int fd);
int sys_read(int fd, void *base, size_t len);
int sys_write(int fd, void *base, size_t len);
int sys_seek(int fd, off_t pos, int whence);
int sys_fstat(int fd, struct stat *stat);
int sys_fsync(int fd);
int sys_getcwd(char *buffer, size_t len);
int sys_getdirentry(int fd, struct dirent *dirent);
int sys_dup(int fd1, int fd2);
void sys_set_priority(uint32_t priority); 
int sys_mkdir(const char *path);
int sys_rm(const char *path);
int sys_chdir(const char *path);
int sys_rename(const char *path1, const char *path2);
int sys_pipe(int *fd_store);
int sys_mkfifo(const char *name, uint32_t open_flags);
int sys_link(const char *path1, const char *path2);
int sys_unlink(const char *path);

int sys_sigaction(int sign, struct sigaction *act, struct sigaction *old);
int sys_sigtkill(int pid, int sign);
int sys_sigprocmask(int how, const sigset_t *set, sigset_t *old);
int sys_sigsuspend(uint32_t mask);

int sys_send_event(int pid, int event_type, int event);
int sys_recv_event(int *pid_store, int event_type, int *event_store, unsigned int timeout);
int sys_mbox_init(unsigned int max_slots);
int sys_mbox_send(int id, struct mboxbuf *buf, unsigned int timeout);
int sys_mbox_recv(int id, struct mboxbuf *buf, unsigned int timeout);
int sys_mbox_free(int id);
int sys_mbox_info(int id, struct mboxinfo *info);
int sys_receive_packet(uint8_t *buf, size_t len, size_t* len_store);
int sys_transmit_packet(uint8_t *buf, size_t len,size_t* len_store);
int sys_ping(char *target, int len);
int sys_process_dump();
int sys_rtdump();
int sys_arpprint();
int sys_netstatus();

int sys_sock_socket(uint32_t type, const char* ipaddr, uint32_t iplen);
int sys_sock_listen(uint32_t tcpfd, uint32_t qsize);
int sys_sock_accept(uint32_t listenfd, uint32_t timeout);
int sys_sock_connect(uint32_t sockfd, const char* ipaddr, uint32_t iplen);
int sys_sock_bind(uint32_t sockfd, uint32_t lport, uint32_t rport);
int sys_sock_send(uint32_t sockfd, char* buf, uint32_t len, uint32_t timeout);
int sys_sock_recv(uint32_t sockfd, char* buf, uint32_t len, uint32_t timeout);
int sys_sock_close(uint32_t sockfd);
int sys_sock_shutdown(uint32_t sockfd, uint32_t type);

#endif /* !__USER_LIBS_SYSCALL_H__ */


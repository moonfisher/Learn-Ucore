#ifndef __USER_LIBS_ULIB_H__
#define __USER_LIBS_ULIB_H__

#include "defs.h"

void __warn(const char *file, int line, const char *fmt, ...);
void __noreturn __panic(const char *file, int line, const char *fmt, ...);

#define warn(...) \
    __warn(__FILE__, __LINE__, __VA_ARGS__)

#define panic(...) \
    __panic(__FILE__, __LINE__, __VA_ARGS__)

#define assert(x)                              \
    do                                         \
    {                                          \
        if (!(x))                              \
        {                                      \
            panic("assertion failed: %s", #x); \
        }                                      \
    } while (0)

// static_assert(x) will generate a compile-time error if 'x' is false.
#define static_assert(x) switch ((int)(x)){case 0 : case (x):; }

int fprintf(int fd, const char *fmt, ...);

void __noreturn exit(int error_code);
int fork(char *name);
int clone(uint32_t clone_flags, uintptr_t stack, int (*fn)(void *), void *arg);
int wait(void);
int waitpid(int pid, int *store);
void yield(void);
int kill(int pid);
int getpid(void);
void print_pgdir(void);
void print_vm(void);
void print_vfs(void);
int mmap(uintptr_t *addr_store, size_t len, uint32_t mmap_flags);
int munmap(uintptr_t addr, size_t len);
int sleep(unsigned int time);
unsigned int gettime_msec(void);
int ptrace(int request, int pid, int addr, int data);

// network
int receive_packet(uint8_t *buf, size_t len);
int transmit_packet(uint8_t *buf, size_t len, size_t *len_store);

int send_event(int pid, int event_type, int event);
int recv_event(int *pid_store, int event_type, int *event_store);
int recv_event_timeout(int *pid_store, int event_type, int *event_store, unsigned int timeout);

struct mboxbuf;
struct mboxinfo;

int mbox_init(unsigned int max_slots);
int mbox_send(int id, struct mboxbuf *buf);
int mbox_send_timeout(int id, struct mboxbuf *buf, unsigned int timeout);
int mbox_recv(int id, struct mboxbuf *buf);
int mbox_recv_timeout(int id, struct mboxbuf *buf, unsigned int timeout);
int mbox_free(int id);
int mbox_info(int id, struct mboxinfo *info);

int ping(char *target, int len);
int process_dump(void);
int rtdump(void);
int arpprint(void);
int netstatus(void);

int __exec(const char *name, const char **argv);

#define __exec0(name, path, ...) \
    ({ const char *argv[] = {path, ##__VA_ARGS__, NULL}; __exec(name, argv); })

#define exec(path, ...) __exec0(NULL, path, ##__VA_ARGS__)
#define nexec(name, path, ...) __exec0(name, path, ##__VA_ARGS__)

void set_priority(uint32_t priority);
void halt(void);
int sock_socket(uint32_t type, const char *ipaddr, uint32_t iplen);
int sock_listen(uint32_t tcpfd, uint32_t qsize);
int sock_accept(uint32_t listenfd, uint32_t timeout);
int sock_connect(uint32_t sockfd, const char *ipaddr, uint32_t iplen);
int sock_bind(uint32_t sockfd, uint32_t lport, uint32_t rport);
int sock_send(uint32_t sockfd, char *buf, uint32_t len, uint32_t timeout);
int sock_recv(uint32_t sockfd, char *buf, uint32_t len, uint32_t timeout);
int sock_close(uint32_t sockfd);
int sock_shutdown(uint32_t sockfd, uint32_t type);

#endif /* !__USER_LIBS_ULIB_H__ */

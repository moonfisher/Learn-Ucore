#ifndef __USER_LIBS_ULIB_H__
#define __USER_LIBS_ULIB_H__

#include "defs.h"

void __warn(const char *file, int line, const char *fmt, ...);
void __noreturn __panic(const char *file, int line, const char *fmt, ...);

#define warn(...)                                       \
    __warn(__FILE__, __LINE__, __VA_ARGS__)

#define panic(...)                                      \
    __panic(__FILE__, __LINE__, __VA_ARGS__)

#define assert(x)                                       \
    do {                                                \
        if (!(x)) {                                     \
            panic("assertion failed: %s", #x);          \
        }                                               \
    } while (0)

// static_assert(x) will generate a compile-time error if 'x' is false.
#define static_assert(x)                                \
    switch ((int)(x)) { case 0: case (x): ; }

int fprintf(int fd, const char *fmt, ...);

void __noreturn exit(int error_code);
int fork(char *name);
int wait(void);
int waitpid(int pid, int *store);
void yield(void);
int kill(int pid);
int getpid(void);
void print_pgdir(void);
int sleep(unsigned int time);
unsigned int gettime_msec(void);
int mmap(uintptr_t * addr_store, size_t len, uint32_t mmap_flags);
int munmap(uintptr_t addr, size_t len);
int clone(uint32_t clone_flags, uintptr_t stack, int (*fn) (void *), void *arg);
int __exec(const char *name, const char **argv);

#define __exec0(name, path, ...)                \
({ const char *argv[] = {path, ##__VA_ARGS__, NULL}; __exec(name, argv); })

#define exec(path, ...)                         __exec0(NULL, path, ##__VA_ARGS__)
#define nexec(name, path, ...)                  __exec0(name, path, ##__VA_ARGS__)

void set_priority(uint32_t priority); 
void halt(void);

#endif /* !__USER_LIBS_ULIB_H__ */


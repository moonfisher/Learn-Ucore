#ifndef __LIBS_X86_H__
#define __LIBS_X86_H__

#include "defs.h"

/*
 https://www.cnblogs.com/nufangrensheng/p/3890856.html
 
 内嵌汇编语法如下：
 __asm__ (汇编语句模板: 输出部分: 输入部分: 破坏描述部分)
 共四个部分：汇编语句模板，输出部分，输入部分，破坏描述部分，各部分使用":"格开，
 汇编语句模板必不可少，其他三部分可选，如果使用了后面的部分，而前面部分为空，也需要用":"格开，
 相应部分内容为空。例如：
 __asm__ __volatile__("cli": : :"memory")
 
 C 语言关键字 volatile:
 注意它是用来修饰变量而不是上面介绍的__volatile__，表明某个变量的值
 可能在外部被改变，因此对这些变量的存取不能缓存到寄存器，每次使用时需要重新存取。
 该关键字在多线程环境下经常使用，因为在编写多线程的程序时，同一个变量可能被多个线程修改，
 而程序通过该变量同步各个线程
 
 memory(内存屏障) 描述符告知 GCC：
 1）不要将该段内嵌汇编指令与前面的指令重新排序；也就是在执行内嵌汇编代码之前，它前面的指令都执行完毕
 2）不要将变量缓存到寄存器，因为这段代码可能会用到内存变量，而这些内存变量会以不可预知的方式发生改变，
 因此 GCC 插入必要的代码先将缓存到寄存器的变量值写回内存，如果后面又访问这些变量，需要重新访问内存。
 如果汇编指令修改了内存，但是 GCC 本身却察觉不到，因为在输出部分没有描述，此时就需要在修改描述部分
 增加 memory，告诉 GCC 内存已经被修改，GCC 得知这个信息后，就会在这段指令之前，插入必要的指令将前面
 因为优化 Cache 到寄存器中的变量值先写回内存，如果以后又要使用这些变量再重新读取。
 使用 volatile 也可以达到这个目的，但是我们在每个变量前增加该关键字，不如使用 memory 方便
*/

#define do_div(n, base) ({                                          \
            unsigned long __upper, __low, __high, __mod, __base;    \
            __base = (base);                                        \
            asm ("" : "=a" (__low), "=d" (__high) : "A" (n));       \
            __upper = __high;                                       \
            if (__high != 0) {                                      \
                __upper = __high % __base;                          \
                __high = __high / __base;                           \
            }                                                       \
            asm ("divl %2" : "=a" (__low), "=d" (__mod)             \
                : "rm" (__base), "0" (__low), "1" (__upper));       \
            asm ("" : "=A" (n) : "a" (__low), "d" (__high));        \
            __mod;                                                  \
        })

#define barrier() __asm__ __volatile__ ("" ::: "memory")

static inline uint8_t inb(uint16_t port) __attribute__((always_inline));
static inline uint16_t inw(uint16_t port) __attribute__((always_inline));
static inline void insl(uint32_t port, void *addr, int cnt) __attribute__((always_inline));
static inline void outb(uint16_t port, uint8_t data) __attribute__((always_inline));
static inline void outw(uint16_t port, uint16_t data) __attribute__((always_inline));
static inline void outsl(uint32_t port, const void *addr, int cnt) __attribute__((always_inline));
static inline uint32_t read_ebp(void) __attribute__((always_inline));
static inline void breakpoint(void) __attribute__((always_inline));
static inline uint32_t read_dr(unsigned regnum) __attribute__((always_inline));
static inline void write_dr(unsigned regnum, uint32_t value) __attribute__((always_inline));

/* Pseudo-descriptors used for LGDT, LLDT(not used) and LIDT instructions. */
struct pseudodesc {
    uint16_t pd_lim;        // Limit
    uintptr_t pd_base;      // Base address
} __attribute__ ((packed));

static inline void lidt(struct pseudodesc *pd) __attribute__((always_inline));
static inline void loadgs(uint16_t v) __attribute__((always_inline));
static inline void sti(void) __attribute__((always_inline));
static inline void cli(void) __attribute__((always_inline));
static inline void ltr(uint16_t sel) __attribute__((always_inline));
static inline uint32_t read_eflags(void) __attribute__((always_inline));
static inline void write_eflags(uint32_t eflags) __attribute__((always_inline));
static inline void lcr0(uintptr_t cr0) __attribute__((always_inline));
static inline void lcr3(uintptr_t cr3) __attribute__((always_inline));
static inline uintptr_t rcr0(void) __attribute__((always_inline));
static inline uintptr_t rcr1(void) __attribute__((always_inline));
static inline uintptr_t rcr2(void) __attribute__((always_inline));
static inline uintptr_t rcr3(void) __attribute__((always_inline));
static inline void invlpg(void *addr) __attribute__((always_inline));

/*********************************ne2k用***********************************/
#if ASM_NO_64
    #define shortSwap(num) ({\
        unsigned short _v;\
        __asm__ __volatile__("xchg %%ah,%%al;":"=a"(_v):"a"(num));\
        _v; \
    })

    #define longSwap(num) ({    \
        unsigned long _v;   \
        __asm__ __volatile__("xchg %%ah,%%al\n\t\t" \
            "mov %%eax,%0\n\t\t"        \
            "shl $16,%0\n\t\t"          \
            "shr $16,%%eax\n\t\t"       \
            "xchg %%ah,%%al\n\t\t"      \
            "or %%eax,%0\n\t\t"         \
            :"=b"(_v)                   \
            :"a"(num));                 \
        _v;                             \
    })

    #define htonl(num) longSwap(num)
    #define ntohl(num) longSwap(num)
    #define htons(num) shortSwap(num)
    #define ntohs(num) shortSwap(num)
#else
    #define htonl(num)  (num)
    #define ntohl(num)  (num)
    #define htons(num)  (num)
    #define ntohs(num)  (num)
#endif

#define X86_FEATURE_APIC    ( 0 * 32 + 9) /* Onboard APIC */

static inline void native_cpuid(uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
    /* ecx is often an input as well as an output. */
    asm volatile("cpuid" : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx) : "0" (*eax), "2" (*ecx) : "memory");
}

/* Should work well enough on modern CPUs for testing */
static inline uint32_t cpu_has_feature(int flag)
{
    uint32_t eax, ebx, ecx, edx;
    
    eax = (flag & 0x100) ? 7 : (flag & 0x20) ? 0x80000001 : 1;
    ecx = 0;
    
    asm volatile("cpuid" : "+a" (eax), "=b" (ebx), "=d" (edx), "+c" (ecx));
    
    return ((flag & 0x100 ? ebx : (flag & 0x80) ? ecx : edx) >> (flag & 31)) & 1;
}

static inline uint32_t inl(int port)
{
    uint32_t data;
    asm volatile("inl %w1,%0" : "=a" (data) : "d" (port));
    return data;
}

static inline void outl(int port, uint32_t data)
{
    asm volatile("outl %0,%w1" : : "a" (data), "d" (port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t data = 0;
#if ASM_NO_64
    asm volatile ("inb %1, %0" : "=a" (data) : "d" (port) : "memory");
#endif
    return data;
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t data = 0;
#if ASM_NO_64
    asm volatile ("inw %1, %0" : "=a" (data) : "d" (port));
#endif
    return data;
}

static inline void insl(uint32_t port, void *addr, int cnt)
{
#if ASM_NO_64
    asm volatile (
        "cld;"
        "repne; insl;"
        : "=D" (addr), "=c" (cnt)
        : "d" (port), "0" (addr), "1" (cnt)
        : "memory", "cc");
#endif
}

static inline void outb(uint16_t port, uint8_t data)
{
#if ASM_NO_64
    asm volatile ("outb %0, %1" :: "a" (data), "d" (port) : "memory");
#endif
}

static inline void outw(uint16_t port, uint16_t data)
{
#if ASM_NO_64
    asm volatile ("outw %0, %1" :: "a" (data), "d" (port) : "memory");
#endif
}

static inline void outsl(uint32_t port, const void *addr, int cnt)
{
#if ASM_NO_64
    asm volatile (
        "cld;"
        "repne; outsl;"
        : "=S" (addr), "=c" (cnt)
        : "d" (port), "0" (addr), "1" (cnt)
        : "memory", "cc");
#endif
}

static inline uint32_t read_ebp(void)
{
    uint32_t ebp = 0;
#if ASM_NO_64
    asm volatile ("movl %%ebp, %0" : "=r" (ebp));
#endif
    return ebp;
}

static inline void breakpoint(void)
{
#if ASM_NO_64
    asm volatile ("int $3");
#endif
}

static inline uint32_t read_dr(unsigned regnum)
{
    uint32_t value = 0;
#if ASM_NO_64
    switch (regnum)
    {
        case 0: asm volatile ("movl %%db0, %0" : "=r" (value)); break;
        case 1: asm volatile ("movl %%db1, %0" : "=r" (value)); break;
        case 2: asm volatile ("movl %%db2, %0" : "=r" (value)); break;
        case 3: asm volatile ("movl %%db3, %0" : "=r" (value)); break;
        case 6: asm volatile ("movl %%db6, %0" : "=r" (value)); break;
        case 7: asm volatile ("movl %%db7, %0" : "=r" (value)); break;
    }
#endif
    return value;
}

static void write_dr(unsigned regnum, uint32_t value)
{
#if ASM_NO_64
    switch (regnum)
    {
        case 0: asm volatile ("movl %0, %%db0" :: "r" (value)); break;
        case 1: asm volatile ("movl %0, %%db1" :: "r" (value)); break;
        case 2: asm volatile ("movl %0, %%db2" :: "r" (value)); break;
        case 3: asm volatile ("movl %0, %%db3" :: "r" (value)); break;
        case 6: asm volatile ("movl %0, %%db6" :: "r" (value)); break;
        case 7: asm volatile ("movl %0, %%db7" :: "r" (value)); break;
    }
#endif
}

static inline void lidt(struct pseudodesc *pd)
{
#if ASM_NO_64
    asm volatile ("lidt (%0)" :: "r" (pd) : "memory");
#endif
}

static inline void loadgs(uint16_t v)
{
    asm volatile("movw %0, %%gs" : : "r" (v));
}

static inline void sti(void)
{
#if ASM_NO_64
    asm volatile ("sti");
#endif
}

static inline void cli(void)
{
#if ASM_NO_64
    asm volatile ("cli" ::: "memory");
#endif
}

static inline void ltr(uint16_t sel)
{
#if ASM_NO_64
    asm volatile ("ltr %0" :: "r" (sel) : "memory");
#endif
}

static inline void lldr(uint16_t sel)
{
#if ASM_NO_64
    asm volatile ("lldr %0" :: "r" (sel) : "memory");
#endif
}

static inline uint32_t read_eflags(void)
{
    uint32_t eflags = 0;
#if ASM_NO_64
    asm volatile ("pushfl; popl %0" : "=r" (eflags));
#endif
    return eflags;
}

static inline void write_eflags(uint32_t eflags)
{
#if ASM_NO_64
    asm volatile ("pushl %0; popfl" :: "r" (eflags));
#endif
}

static inline void lcr0(uintptr_t cr0)
{
#if ASM_NO_64
    asm volatile ("mov %0, %%cr0" :: "r" (cr0) : "memory");
#endif
}

static inline void lcr3(uintptr_t cr3)
{
#if ASM_NO_64
    asm volatile ("mov %0, %%cr3" :: "r" (cr3) : "memory");
#endif
}

static inline uintptr_t rcr0(void)
{
    uintptr_t cr0 = 0;
#if ASM_NO_64
    asm volatile ("mov %%cr0, %0" : "=r" (cr0) :: "memory");
#endif
    return cr0;
}

static inline uintptr_t rcr1(void)
{
    uintptr_t cr1 = 0;
#if ASM_NO_64
    asm volatile ("mov %%cr1, %0" : "=r" (cr1) :: "memory");
#endif
    return cr1;
}

static inline uintptr_t rcr2(void)
{
    uintptr_t cr2 = 0;
#if ASM_NO_64
    asm volatile ("mov %%cr2, %0" : "=r" (cr2) :: "memory");
#endif
    return cr2;
}

static inline uintptr_t rcr3(void)
{
    uintptr_t cr3 = 0;
#if ASM_NO_64
    asm volatile ("mov %%cr3, %0" : "=r" (cr3) :: "memory");
#endif
    return cr3;
}

static inline void invlpg(void *addr)
{
#if ASM_NO_64
    asm volatile ("invlpg (%0)" :: "r" (addr) : "memory");
#endif
}

static inline uint32_t xchg(volatile uint32_t *addr, uint32_t newval)
{
    uint32_t result;
    
    // The + in "+m" denotes a read-modify-write operand.
    // 这里使用 lock 指令，会锁住总线，其它 cpu 无法访问内存，保证原子操作
    asm volatile("lock; xchgl %0, %1"
                 : "+m"(*addr), "=a"(result)
                 : "1"(newval)
                 : "cc");
    return result;
}

static inline int __strcmp(const char *s1, const char *s2) __attribute__((always_inline));
static inline char *__strcpy(char *dst, const char *src) __attribute__((always_inline));
static inline void *__memset(void *s, char c, size_t n) __attribute__((always_inline));
static inline void *__memmove(void *dst, const void *src, size_t n) __attribute__((always_inline));
static inline void *__memcpy(void *dst, const void *src, size_t n) __attribute__((always_inline));

#ifndef __HAVE_ARCH_STRCMP
#define __HAVE_ARCH_STRCMP
static inline int __strcmp(const char *s1, const char *s2)
{
    int ret = 0;
#if ASM_NO_64
    int d0, d1;
    asm volatile (
        "1: lodsb;"
        "scasb;"
        "jne 2f;"
        "testb %%al, %%al;"
        "jne 1b;"
        "xorl %%eax, %%eax;"
        "jmp 3f;"
        "2: sbbl %%eax, %%eax;"
        "orb $1, %%al;"
        "3:"
        : "=a" (ret), "=&S" (d0), "=&D" (d1)
        : "1" (s1), "2" (s2)
        : "memory");
#endif
    return ret;
}

#endif /* __HAVE_ARCH_STRCMP */

#ifndef __HAVE_ARCH_STRCPY
#define __HAVE_ARCH_STRCPY
static inline char *__strcpy(char *dst, const char *src)
{
#if ASM_NO_64
    int d0, d1, d2;
    asm volatile (
        "1: lodsb;"
        "stosb;"
        "testb %%al, %%al;"
        "jne 1b;"
        : "=&S" (d0), "=&D" (d1), "=&a" (d2)
        : "0" (src), "1" (dst) : "memory");
#endif
    return dst;
}
#endif /* __HAVE_ARCH_STRCPY */

#ifndef __HAVE_ARCH_MEMSET
#define __HAVE_ARCH_MEMSET
static inline void *__memset(void *s, char c, size_t n)
{
#if ASM_NO_64
    int d0, d1;
    asm volatile (
        "rep; stosb;"
        : "=&c" (d0), "=&D" (d1)
        : "0" (n), "a" (c), "1" (s)
        : "memory");
#endif
    return s;
}
#endif /* __HAVE_ARCH_MEMSET */

#ifndef __HAVE_ARCH_MEMMOVE
#define __HAVE_ARCH_MEMMOVE
static inline void *__memmove(void *dst, const void *src, size_t n)
{
    if (dst < src)
    {
        return __memcpy(dst, src, n);
    }
    
#if ASM_NO_64
    int d0, d1, d2;
    asm volatile (
        "std;"
        "rep; movsb;"
        "cld;"
        : "=&c" (d0), "=&S" (d1), "=&D" (d2)
        : "0" (n), "1" (n - 1 + src), "2" (n - 1 + dst)
        : "memory");
#endif
    return dst;
}
#endif /* __HAVE_ARCH_MEMMOVE */

#ifndef __HAVE_ARCH_MEMCPY
#define __HAVE_ARCH_MEMCPY
static inline void *__memcpy(void *dst, const void *src, size_t n)
{
#if ASM_NO_64
    int d0, d1, d2;
    asm volatile (
        "rep; movsl;"
        "movl %4, %%ecx;"
        "andl $3, %%ecx;"
        "jz 1f;"
        "rep; movsb;"
        "1:"
        : "=&c" (d0), "=&D" (d1), "=&S" (d2)
        : "0" (n / 4), "g" (n), "1" (dst), "2" (src)
        : "memory");
#endif
    return dst;
}
#endif /* __HAVE_ARCH_MEMCPY */

#endif /* !__LIBS_X86_H__ */


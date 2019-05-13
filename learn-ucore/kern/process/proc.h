#ifndef __KERN_PROCESS_PROC_H__
#define __KERN_PROCESS_PROC_H__

#include "defs.h"
#include "list.h"
#include "trap.h"
#include "memlayout.h"
#include "skew_heap.h"


// process's state in his life cycle
enum proc_state
{
    PROC_UNINIT = 0,  // uninitialized
    PROC_SLEEPING,    // sleeping
    PROC_RUNNABLE,    // runnable(maybe running)
    PROC_ZOMBIE,      // almost dead, and wait parent proc to reclaim his resource
};

// Saved registers for kernel context switches.
// Don't need to save all the %fs etc. segment registers,
// because they are constant across kernel contexts.
// Save all the regular registers so we don't need to care
// which are caller save, but not the return register %eax.
// (Not saving %eax just simplifies the switching code.)
// The layout of context must match code in switch.S.
// 用户在发生进程切换的时候（switch_to），保存当前进程执行的现场，后续好恢复
// 只需要保留几个常用的寄存器就行了，并不需要保存所有 cpu 寄存器
struct context
{
    uint32_t eip;
    uint32_t esp;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
};

#define PROC_NAME_LEN               50
#define MAX_PROCESS                 4096
#define MAX_PID                     (MAX_PROCESS * 2)

extern list_entry_t proc_list;

struct inode;

struct proc_struct
{
    enum proc_state state;                      // Process state
    int pid;                                    // Process ID
    int runs;                                   // the running times of Proces 进程运行过的次数
/*
    kstack: 每个进程都有一个内核桟，并且位于内核地址空间的不同位置。
    对于内核线程，该桟就是运行时的程序使用的桟；
    而对于普通进程，该桟是发生特权级改变的时候使保存被打断的硬件信息用的桟。
    Ucore 在创建进程时分配了 2 个连续的物理页作为内核栈的空间。
    这个桟很小，所以内核中的代码应该尽可能的紧凑，并且避免在桟上分配大的数据结构，以免桟溢出，导致系统崩溃。
    kstack 记录了分配给该进程/线程的内核桟的位置。
    主要作用有以下几点:
    1) 当内核准备从一个进程切换到另一个的时候，需要根据 kstack 的值正确的设置好 tss ，
       以便在进程切换以后再发生中断时能够使用正确的桟。
    2) 内核桟位于内核地址空间，并且是不共享的（每个进程/线程都拥有自己的内核桟），因此不受到 mm 的管理，
       当进程退出的时候，内核能够根据 kstack 的值快速定位桟的位置并进行回收。
 */
    uintptr_t kstack;                           // Process kernel stack
    volatile bool need_resched;                 // bool value: need to be rescheduled to release CPU?
/*
    parent：用户进程的父进程（创建它的进程）。在所有进程中，只有一个进程没有父进程，
    就是内核创建的第一个内核线程 idleproc。内核根据这个父子关系建立进程的树形结构，用于维护一些特殊的操作，
    例如确定哪些进程是否可以对另外一些进程进行什么样的操作等等。
 */
    struct proc_struct *parent;                 // the parent process
/*
    mm：内存管理的信息，包括内存映射列表、页表指针等,即实验三中的描述进程虚拟内存的结构体.
    mm 里有个很重要的项 pgdir，记 录的是该进程使用的一级页表的物理地址。
    这个主要给用户进程使用，内核线程不需要使用 mm，因为内核的地址映射是一开始就静态映射好了
    内核代码都在内存，不需要也不能 swap 交换到硬盘上去，所以内核进程不需要 mm 结构
 */
    struct mm_struct *mm;                       // Process's memory management field
/*
    context：进程的上下文，用于进程切换（switch_to）。 在 ucore 中，所有的进程在内核中也是相对独立的
    （例如独立的内核堆栈以及上下文等等）。使用 context 保存寄存器的目的就在于在内核态中能够进行上下文之间的切换。
    context 和下面的 tf 有相似的地方，都会因为被切换而保存 cpu 寄存器状态数据，但区别是 context 仅在
    进程调度切换的场景下使用，tf 是用在进程从用户态切换到内核态，这时候进程不一定发生了切换
 */
    struct context context;                     // Switch here to run process
/*
    tf：中断帧的指针，总是指向内核栈的某个位置：
    当进程从用户空间跳到内核空间时，中断帧记录了进程在被中断前的状态。
    当内核需要跳回用户空间时，需要调整中断帧以恢复让进程继续执行的各寄存器值。
    除此之外，ucore 内核允许嵌套中断。因此为了保证嵌套中断发生时 tf 总是能够指向当前的 trapframe，
    ucore 在内核桟上维护了 tf 的链。
*/
    struct trapframe *tf;                       // Trap frame for current interrupt
/*
    cr3 保存页表的物理地址，目的就是进程切换的时候方便直接使用 lcr3 实现页表切换，
    避免每次都根据 mm 来计算 cr3。 mm 数据结构是用来实现用户空间的虚存管理的，但是内核线程没有用户空间，
    它执行的只是内核中的一小段代码（通常是一小段函数），所以它没有 mm 结构，也就是 NULL。
    当某个进程是一个普通用户态进程的时候， PCB 中的 cr3 就是 mm 中页表（ pgdir）的物理地址；
    而当它是内核线程的时候，cr3 等于 boot_cr3。 而 boot_cr3 指向了 ucore 启动时建立好的栈内核虚拟空间的页目
    录表首地址。
*/
    uintptr_t cr3;                              // CR3: base addr of Page Directroy Table(PDT)
    uint32_t flags;                             // Process flag
    char name[PROC_NAME_LEN + 1];               // Process name
    list_entry_t list_link;                     // Process link list
    list_entry_t hash_link;                     // Process hash list
    // 记录进程退出的原因，这个需要返回给父进程使用
    int exit_code;                              // exit code (be sent to parent proc)
    // 记录当前进程是因为什么原因处于等待状态
    uint32_t wait_state;                        // waiting state
    // cptr-->指向最新创建的子进程
    // yptr-->指向同一个父进程下比自己后创建的子进程
    // optr-->指向同一个父进程下比自己先创建的子进程
    struct proc_struct *cptr, *yptr, *optr;     // relations between processes
    struct run_queue *rq;                       // running queue contains Process
    // 该进程的调度链表结构，该结构内部的连接组成了 运行队列 列表
    list_entry_t run_link;                      // the entry linked in run queue
    // 进程运行时间片
    int time_slice;                             // time slice for occupying the CPU
    // 该进程在优先队列中的节点
    skew_heap_entry_t run_pool;            // the entry in the run pool
    // 该进程的调度步进值
    uint32_t stride;                       // the current stride of the process
    // 该进程的调度优先级
    uint32_t priority;                     // the priority of process, set by set_priority(uint32_t)
    // 进程访问文件系统的接口
    struct files_struct *filesp;                // the file related info(pwd, files_count, files_array, fs_semaphore) of process
};

#define PF_EXITING                  0x00000001      // getting shutdown

#define WT_INTERRUPTED               0x80000000                    // the wait state could be interrupted
#define WT_CHILD                    (0x00000001 | WT_INTERRUPTED)  // wait child process
#define WT_KSEM                      0x00000100                    // wait kernel semaphore
#define WT_TIMER                    (0x00000002 | WT_INTERRUPTED)  // wait timer
#define WT_KBD                      (0x00000004 | WT_INTERRUPTED)  // wait the input of keyboard

#define le2proc(le, member)         \
    to_struct((le), struct proc_struct, member)

extern struct proc_struct *idleproc, *initproc, *current;

void proc_init(void);
void proc_run(struct proc_struct *proc);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags, const char *name);

char *set_proc_name(struct proc_struct *proc, const char *name);
char *get_proc_name(struct proc_struct *proc);
void cpu_idle(void) __attribute__((noreturn));

struct proc_struct *find_proc(int pid);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf, const char *name);
int do_exit(int error_code);
int do_yield(void);
int do_execve(const char *name, int argc, const char **argv);
int do_wait(int pid, int *code_store);
int do_kill(int pid);
int do_mmap(uintptr_t * addr_store, size_t len, uint32_t mmap_flags);
int do_munmap(uintptr_t addr, size_t len);
// set the process's priority (bigger value will get more CPU time)
void set_priority(uint32_t priority);
int do_sleep(unsigned int time);
#endif /* !__KERN_PROCESS_PROC_H__ */


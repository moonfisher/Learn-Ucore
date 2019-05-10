#include "proc.h"
#include "kmalloc.h"
#include "string.h"
#include "sync.h"
#include "pmm.h"
#include "error.h"
#include "sched.h"
#include "elf.h"
#include "vmm.h"
#include "trap.h"
#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include "unistd.h"
#include "fs.h"
#include "vfs.h"
#include "sysfile.h"
#include "stat.h"

/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files, etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc, 
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:
                                            
  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+ 
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  + 
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep 
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit   
SYS_getpid      : get the process's pid

*/

// the process set's list 0xC015C37C
list_entry_t proc_list;

#define HASH_SHIFT          10
#define HASH_LIST_SIZE      (1 << HASH_SHIFT)
#define pid_hashfn(x)       (hash32(x, HASH_SHIFT))

// has list for process set based on pid 0xC0159060
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle 进程 pid = 0，是系统创建的第一个进程（也是内核线程），没有父进程，
// 也是唯一一个没有通过 fork 或者 kernel_thread 产生的进程。没有函数入口。
// idle 创建之后，很快用 kernel_thread 创建了 pid = 1 的 init 内核线程
// idle 只负责 schedule 调度
struct proc_struct *idleproc = NULL;    // 0xC0159040

// init 进程 pid = 1，是系统创建的第二个进程（也是内核线程），父进程是 idle，
// init 入口函数是 init_main，在启动 shell 之后，后续只负责等待进程结束做清理工作
struct proc_struct *initproc = NULL;    // 0xC0159044

// current proc 标记当前正在运行的进程，相当于全局变量，很多地方都需要获取当前正在运行的进程
struct proc_struct *current = NULL;     // 0xC0159048

static int nr_process = 0;

#if ASM_NO_64
    void kernel_thread_entry(void);
    void forkrets(struct trapframe *tf);
    void switch_to(struct context *from, struct context *to);
#else
    void kernel_thread_entry(void) {}
    void forkrets(struct trapframe *tf) {}
    void switch_to(struct context *from, struct context *to) {}
#endif

// alloc_proc - alloc a proc_struct and init all fields of proc_struct
static struct proc_struct *alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL)
    {
        proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->runs = 0;
        proc->kstack = 0;
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL;
        memset(&(proc->context), 0, sizeof(struct context));
        proc->tf = NULL;
        proc->cr3 = boot_cr3;   // 页目录要用物理地址 0x156000
        proc->flags = 0;
        memset(proc->name, 0, PROC_NAME_LEN);
        proc->wait_state = 0;
        proc->cptr = proc->optr = proc->yptr = NULL;
        proc->rq = NULL;
        list_init(&(proc->run_link));
        proc->time_slice = 0;
        proc->run_pool.left = proc->run_pool.right = proc->run_pool.parent = NULL;
        proc->stride = 0;
        proc->priority = 0;
        proc->filesp = NULL;
    }
    return proc;
}

// set_proc_name - set the name of proc
char *set_proc_name(struct proc_struct *proc, const char *name)
{
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
char *get_proc_name(struct proc_struct *proc)
{
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// set_links - set the relation links of process
static void set_links(struct proc_struct *proc)
{
    list_add(&proc_list, &(proc->list_link));
    proc->yptr = NULL;
    proc->optr = proc->parent->cptr;
    if (proc->optr != NULL)
    {
        proc->optr->yptr = proc;
    }
    proc->parent->cptr = proc;
    nr_process++;
}

// remove_links - clean the relation links of process
static void remove_links(struct proc_struct *proc)
{
    list_del(&(proc->list_link));
    if (proc->optr != NULL)
    {
        proc->optr->yptr = proc->yptr;
    }
    if (proc->yptr != NULL)
    {
        proc->yptr->optr = proc->optr;
    }
    else
    {
       proc->parent->cptr = proc->optr;
    }
    nr_process--;
}

// get_pid - alloc a unique pid for process
static int get_pid(void)
{
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++ last_pid >= MAX_PID)
    {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe)
    {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list)
        {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid)
            {
                if (++ last_pid >= next_safe)
                {
                    if (last_pid >= MAX_PID)
                    {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid)
            {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}

// proc_run - make process "proc" running on cpu
// NOTE: before call switch_to, should load  base addr of "proc"'s new PDT
/*
 设置任务状态段 ts 中特权态 0 下的栈顶指针 esp0 为 next 内核线程的内核栈的栈顶，
 即 next->kstack + KSTACKSIZE；
 设置 CR3 寄存器的值为 next 内核线程的页目录表起始地址 next->cr3，
 这实际上是完成进程间的页表切换；
 由 switch_to 函数完成具体的两个线程的执行现场切换，即切换各个寄存器，当 switch_to 函数执行
 完 ret 指令后，就切换到 next 执行了。
 在页表设置方面，考虑到以后的进程有各自的页表，其起始地址各不相同，只有完成页表切换，才能确保新的
 进程能够正确执行。
 */
void proc_run(struct proc_struct *proc)
{
    if (proc != current)
    {
        char *name = get_proc_name(proc);
        cprintf("proc_run: pid = %d, name = \"%s\", runs = %d.\n", proc->pid, name, proc->runs);
        
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;
        local_intr_save(intr_flag);
        {
            current = proc;
            // 重新设置当前进程内核栈顶，这里实际上等于说进程每次切换之后，内核栈都重新回到栈顶的位置
            // 那么之前进程在进入内核态之后在内核栈里用到的数据，此时都会清空
            // 内核栈的地址通过全局 tss 获取的，进程切换的时候更新 tss。
            load_esp0(next->kstack + KSTACKSIZE);
            lcr3(next->cr3);
            switch_to(&(prev->context), &(next->context));
        }
        local_intr_restore(intr_flag);
    }
}

// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
/*
 在发生进程切换 switch_to 之后， 切换后的进程函数入口就是 forkret，这并非是进程上次执行的现场
 这里会根据进程之前构造的 trapframe 中断桢的内容，来修改各个寄存器的值(包括段寄存器 cs)，
 最终实现切换。无论是切换到用户进程，还是内核进程，流程是一样的，只是各个进程自己的 trapframe 结构不同
*/
static void forkret(void)
{
    cprintf("forkret: pid = %d, name = \"%s\", kstack = %x.\n", current->pid, current->name, current->kstack);
    print_trapframe(current->tf);
    forkrets(current->tf);
}

// hash_proc - add proc into proc hash_list
static void hash_proc(struct proc_struct *proc)
{
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// unhash_proc - delete proc from proc hash_list
static void unhash_proc(struct proc_struct *proc)
{
    list_del(&(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
struct proc_struct *find_proc(int pid)
{
    if (0 < pid && pid < MAX_PID)
    {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list)
        {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid)
            {
                return proc;
            }
        }
    }
    return NULL;
}

// kernel_thread - create a kernel thread using "fn" function
// NOTE: the contents of temp trapframe tf will be copied to 
//       proc->tf in do_fork-->copy_thread function
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags, const char *name)
{
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));
    
    // 通过 kernel_thread 创建的内核线程，代码段都是内核段
    tf.tf_cs = KERNEL_CS;
    tf.tf_ds = tf.tf_es = tf.tf_ss = KERNEL_DS;
    
    // 设置下次要启动的函数，调度之前 ebx 中存有函数地址
    tf.tf_regs.reg_ebx = (uint32_t)fn;
    
    // 参数，调度之前参数的地址存于 edx
    tf.tf_regs.reg_edx = (uint32_t)arg;
    
    // 下次进程运行的位置
    tf.tf_eip = (uint32_t)kernel_thread_entry;  // 0xC010b37b
    
    return do_fork(clone_flags | CLONE_VM, 0, &tf, name);
}

// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
// 分配内核栈空间，2个页面，8k
static int setup_kstack(struct proc_struct *proc)
{
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL)
    {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

// put_kstack - free the memory space of process kernel stack
static void put_kstack(struct proc_struct *proc)
{
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// setup_pgdir - alloc one page as PDT
static int setup_pgdir(struct mm_struct *mm)
{
    struct Page *page;
    if ((page = alloc_page()) == NULL)
    {
        return -E_NO_MEM;
    }
    pde_t *pgdir = page2kva(page);
    // 先拷贝内核地址映射表，这样即使后续切换到用户进程页表，内核地址空间访问也还是正常的
    memcpy(pgdir, boot_pgdir, PGSIZE);
    // 这里并没有设置 PTE_U 用户访问权限，所以在用户态下，无法直接访问内核地址空间，
    // 会触发 T_PGFLT 中断
    pgdir[PDX(VPT)] = PADDR(pgdir) | PTE_P | PTE_W;
    mm->pgdir = pgdir;
    return 0;
}

// put_pgdir - free the memory space of PDT
static void put_pgdir(struct mm_struct *mm)
{
    free_page(kva2page(mm->pgdir));
}

// copy_mm - process "proc" duplicate OR share process "current"'s mm according clone_flags
//         - if clone_flags & CLONE_VM, then "share" ; else "duplicate"
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
    struct mm_struct *mm, *oldmm = current->mm;

    /* current is a kernel thread */
    if (oldmm == NULL)
    {
        return 0;
    }
    
    // 如果是 CLONE_VM 说明是多 task 共享地址空间，这就类似于多线程的模型
    if (clone_flags & CLONE_VM)
    {
        mm = oldmm;
        goto good_mm;
    }

    int ret = -E_NO_MEM;
    if ((mm = mm_create()) == NULL)
    {
        goto bad_mm;
    }
    
    if (setup_pgdir(mm) != 0)
    {
        goto bad_pgdir_cleanup_mm;
    }

    // 打开互斥锁，避免多个进程同时访问内存
    lock_mm(oldmm);
    {
        ret = dup_mmap(mm, oldmm);
    }
    unlock_mm(oldmm);

    if (ret != 0)
    {
        goto bad_dup_cleanup_mmap;
    }

good_mm:
    mm_count_inc(mm);
    proc->mm = mm;
    proc->cr3 = PADDR(mm->pgdir);
    return 0;
bad_dup_cleanup_mmap:
    exit_mmap(mm);
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    return ret;
}

// copy_thread - setup the trapframe on the  process's kernel stack top and
//             - setup the kernel entry point and stack of process
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)
{
    // 在内核堆栈的顶部设置中断帧大小的一块栈空间，用于存放中断桢的数据
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    // 拷贝在 kernel_thread 函数建立的临时中断帧的初始值
    *(proc->tf) = *tf;
    // 设置子进程/线程执行完 do_fork 后的返回值
    proc->tf->tf_regs.reg_eax = 0;
    // 设置中断帧中的栈指针 esp
    proc->tf->tf_esp = esp;
    // 使能中断
    proc->tf->tf_eflags |= FL_IF;
    // 设置好新进程的入口地址
    proc->context.eip = (uintptr_t)forkret;
    // 更新上下文sp指针位置
    proc->context.esp = (uintptr_t)(proc->tf);
}

//copy_fs & put_fs function used by do_fork
static int copy_fs(uint32_t clone_flags, struct proc_struct *proc)
{
    struct files_struct *filesp, *old_filesp = current->filesp;
    assert(old_filesp != NULL);

    if (clone_flags & CLONE_FS)
    {
        filesp = old_filesp;
        goto good_files_struct;
    }

    int ret = -E_NO_MEM;
    if ((filesp = files_create()) == NULL)
    {
        goto bad_files_struct;
    }

    if ((ret = dup_fs(filesp, old_filesp)) != 0)
    {
        goto bad_dup_cleanup_fs;
    }

good_files_struct:
    files_count_inc(filesp);
    proc->filesp = filesp;
    return 0;

bad_dup_cleanup_fs:
    files_destroy(filesp);
bad_files_struct:
    return ret;
}

static void put_fs(struct proc_struct *proc)
{
    struct files_struct *filesp = proc->filesp;
    if (filesp != NULL)
    {
        if (files_count_dec(filesp) == 0)
        {
            files_destroy(filesp);
        }
    }
}

/* do_fork -     parent process for a new child process
 * @clone_flags: used to guide how to clone the child process
 * @stack:       the parent's user stack pointer. if stack==0, It means to fork a kernel thread.
 * @tf:          the trapframe info, which will be copied to child process's proc->tf
 */
/*
 内核不区分线程还是进程，都同样是 proc_struct，一样的调度过程
 区别是在于 clone_flags 为 CLONE_VM 或者 CLONE_FS
 如果多个 proc_struct 之间可以共享虚拟地址和文件句柄，那可以把这些 proc_struct 看做
 是同时属于同一个进程的多个线程
 如果多个 proc_struct 之间不共享虚拟地址和文件句柄，那可以把这些 proc_struct 看做
 独立的进程
 
 fork() 工作的机制：
 对父进程的所有值都拷贝一份到子进程（包包括缓冲区的东西），但是拷贝过后，父/子进程对数据的操作是互相不影响，
 也就是说，他们是独立的，但是有一点就是：关于文件的操作有点特殊，对于文件的操作，他们是这样工作的，
 比如在父进程 open 一个文件，那么就会有一个文件描述符并且该文件描述符会有一个条目，并且在文件系统中也有相应的条目，
 当创建一个子进程时，文件描述符会自增一个条目，当在父/子进程调用了 close 函数时，文件描述符就会自减 1，
 但是另一个的进程还可对该文件描述符进行操作，直到文件描述符的条目自减到 0 时，才关闭了文件描述符的作用。
*/
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf, const char *name)
{
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS)
    {
        goto fork_out;
    }
    ret = -E_NO_MEM;
    
    if ((proc = alloc_proc()) == NULL)
    {
        goto fork_out;
    }

    // 设置父节点为当前进程，如果是 shell 下执行的命令，current 就是 sh
    proc->parent = current;
    // 当前进程不可能处于阻塞等待的状态下，还在执行 fork
    assert(current->wait_state == 0);

    // 分配内核栈空间，2个页面，8k大小，供进程切换到内核态之后使用
    if (setup_kstack(proc) != 0)
    {
        goto bad_fork_cleanup_proc;
    }
    
    // 对于文件操作来说，父子进程是共享的，父进程在 fork 之前，关联的文件子进程都能继承，
    // 父子进程指向同样的文件节点，只是文件节点的引用次数增加
    if (copy_fs(clone_flags, proc) != 0)
    {
        goto bad_fork_cleanup_kstack;
    }
    
    // 调用 copy_mm() 函数复制父进程的内存信息到子进程
    if (copy_mm(clone_flags, proc) != 0)
    {
        goto bad_fork_cleanup_fs;
    }
    
    char local_name[PROC_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));
    snprintf(local_name, sizeof(local_name), "%s", name);
    set_proc_name(proc, local_name);
    
    // 调用copy_thread()函数复制父进程的中断帧和上下文信息
    copy_thread(proc, stack, tf);

    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        set_links(proc);
    }
    local_intr_restore(intr_flag);

    cprintf("do_fork: name = \"%s\", kstack = %x.\n", local_name, current->kstack);
    wakeup_proc(proc);

    ret = proc->pid;
fork_out:
    return ret;

bad_fork_cleanup_fs:
    put_fs(proc);
bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}

// do_exit - called by sys_exit
//   1. call exit_mmap & put_pgdir & mm_destroy to free the almost all memory space of process
//   2. set process' state as PROC_ZOMBIE, then call wakeup_proc(parent) to ask parent reclaim itself.
//   3. call scheduler to switch to other process
int do_exit(int error_code)
{
    // idleproc 和 initproc 进程是不能退出的
    if (current == idleproc)
    {
        panic("idleproc exit.\n");
    }
    
    if (current == initproc)
    {
        panic("initproc exit.\n");
    }
    
    struct mm_struct *mm = current->mm;
    if (mm != NULL)
    {
        // 当前进程退出之后，先加载内核页目录，要用物理地址
        lcr3(boot_cr3);
        if (mm_count_dec(mm) == 0)
        {
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);
        }
        current->mm = NULL;
    }
    put_fs(current);
    current->state = PROC_ZOMBIE;
    current->exit_code = error_code;
    
    bool intr_flag;
    struct proc_struct *proc;
    local_intr_save(intr_flag);
    {
        proc = current->parent;
        // 如果父进程在等待子进程退出，就唤醒父进程
        if (proc->wait_state == WT_CHILD)
        {
            wakeup_proc(proc);
        }
        
        // 如果退出的进程还包含子进程，后续 init 进程将直接接管这些子进程
        // 这些子进程将和 user_main (shell) 平级
        while (current->cptr != NULL)
        {
            proc = current->cptr;
            current->cptr = proc->optr;
    
            proc->yptr = NULL;
            proc->optr = initproc->cptr;
            if (proc->optr != NULL)
            {
                initproc->cptr->yptr = proc;
            }
            // 进程退出之后，会被 init 进程接管，随后 init 进程被唤醒来释放进程相关资源
            proc->parent = initproc;
            initproc->cptr = proc;
            if (proc->state == PROC_ZOMBIE)
            {
                // 如果父进程在等待子进程退出，就唤醒父进程
                if (initproc->wait_state == WT_CHILD)
                {
                    wakeup_proc(initproc);
                }
            }
        }
    }
    local_intr_restore(intr_flag);
    
    cprintf("proc exit: pid = %d, name = \"%s\", error = %d - %e.\n", current->pid, current->name, error_code, error_code);
    schedule();
    // 这下面的代码不会走到，是因为重新调度到 init 进程之后，当前进程资源已经被 init 释放，相关代码在内存已经不存在
    panic("do_exit will not return!! %d.\n", current->pid);
}

//load_icode_read is used by load_icode
static int load_icode_read(int fd, void *buf, size_t len, off_t offset)
{
    int ret;
    if ((ret = sysfile_seek(fd, offset, LSEEK_SET)) != 0)
    {
        return ret;
    }
    if ((ret = sysfile_read(fd, buf, len)) != len)
    {
        return (ret < 0) ? ret : -1;
    }
    return 0;
}

/*
 用户进程入口地址是 0x800020，其下面的一部分地址可用于检测空指针什么的
 各个段的相对位置，可以查看一下 user.ld 文件．
 1，调用 mm_create 函数来申请进程的内存管理数据结构 mm 所需内存空间，并对 mm 进行初始化;
 2，调用 setup_pgdir 来申请一个页目录表所需的一个页大小的内存空间，并把描述 ucore 内核虚空间
 映射的内核页表 (boot_pgdir所指) 的内容拷贝到此新目录表中，最后让 mm->pgdir 指向此页目录表，
 这就是进程新的页目录表了，且能够正确映射内核虚空间;
 3，根据应用程序执行码的起始位置来解析此 ELF 格式的执行程序，并调用 mm_map 函数根据 ELF 格式的
 执行程序说明的各个段 (代码段、数据段、BSS段等) 的起始位置和大小建立对应的 vma 结构，并把 vma 插
 入到 mm 结构中，从而表明了用户进程的合法用户态虚拟地址空间;
 4，调用根据执行程序各个段的大小分配物理内存空间，并根据执行程序各个段的起始位置确定虚拟地址，并在页
 表中建立好物理地址和虚拟地址的映射关系，然后把执行程序各个段的内容拷贝到相应的内核虚拟地址中，至此应
 用程序执行码和数据已经根据编译时设定地址放置到虚拟内存中了;
 5，需要给用户进程设置用户栈，为此调用 mm_mmap 函数建立用户栈的 vma 结构，明确用户栈的位置在用户
 虚空间的顶端，大小为 256 个页，即 1MB，并分配一定数量的物理内存且建立好栈的 虚地址<-->物理地址 映射关系;
 6，至此，进程内的内存管理 vma 和 mm 数据结构已经建立完成，于是把 mm->pgdir 赋值到 cr3 寄存器中，
 即更新了用户进程的虚拟内存空间，此时的 initproc 已经被 sh 的代码和数据覆盖，成为了第一个用户进程，
 但此时这个用户进程的执行现场还没建立好;
 7，先清空进程的中断帧，再重新设置进程的中断帧，使得在执行中断返回指令 iret 后，能够让 CPU 转到用户态
 特权级，并回到用户态内存空间，使用用户态的代码段，数据段和堆栈，且能够跳转到用户进程的第一条指令执行
 ,并确保在用户态能够响应中断;
 */
static int load_icode(int fd, int argc, char **kargv)
{
    assert(argc >= 0 && argc <= EXEC_MAX_ARG_NUM);

    // 走到这里来的时候，页表资源应该已经在上层函数释放过了才对
    if (current->mm != NULL)
    {
        panic("load_icode: current->mm must be empty.\n");
    }

    int ret = -E_NO_MEM;
    struct mm_struct *mm;
    if ((mm = mm_create()) == NULL)
    {
        goto bad_mm;
    }
    
    if (setup_pgdir(mm) != 0)
    {
        goto bad_pgdir_cleanup_mm;
    }

    struct Page *page = NULL;

    struct elfhdr __elf, *elf = &__elf;
    if ((ret = load_icode_read(fd, elf, sizeof(struct elfhdr), 0)) != 0)
    {
        goto bad_elf_cleanup_pgdir;
    }

    if (elf->e_magic != ELF_MAGIC)
    {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir;
    }

    struct proghdr __ph, *ph = &__ph;
    uint32_t vm_flags, perm, phnum;
    for (phnum = 0; phnum < elf->e_phnum; phnum++)
    {
        off_t phoff = elf->e_phoff + sizeof(struct proghdr) * phnum;
        if ((ret = load_icode_read(fd, ph, sizeof(struct proghdr), phoff)) != 0)
        {
            goto bad_cleanup_mmap;
        }
        if (ph->p_type != ELF_PT_LOAD)
        {
            continue;
        }
        if (ph->p_filesz > ph->p_memsz)
        {
            ret = -E_INVAL_ELF;
            goto bad_cleanup_mmap;
        }
        if (ph->p_filesz == 0)
        {
            continue ;
        }
        
        vm_flags = 0;
        // 用户进程，所有页表权限都要设置为 PTE_U，否则用户态访问地址出错
        perm = PTE_U;
        if (ph->p_flags & ELF_PF_X)
        {
            vm_flags |= VM_EXEC;
        }
        if (ph->p_flags & ELF_PF_W)
        {
            vm_flags |= VM_WRITE;
        }
        if (ph->p_flags & ELF_PF_R)
        {
            vm_flags |= VM_READ;
        }
        if (vm_flags & VM_WRITE)
        {
            perm |= PTE_W;
        }
        
        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0)
        {
            goto bad_cleanup_mmap;
        }
        
        off_t offset = ph->p_offset;
        size_t off, size;
        uintptr_t start = ph->p_va, end, la = ROUNDDOWN(start, PGSIZE);

        ret = -E_NO_MEM;

        // 用户进程空间地址映射，根据实际用到的虚拟地址来创建页表映射，并非映射整个 4G 地址空间
        end = ph->p_va + ph->p_filesz;
        while (start < end)
        {
            if ((page = pgdir_alloc_page(mm, la, perm)) == NULL)
            {
                ret = -E_NO_MEM;
                goto bad_cleanup_mmap;
            }
            off = start - la; size = PGSIZE - off; la += PGSIZE;
            if (end < la)
            {
                size -= la - end;
            }
            if ((ret = load_icode_read(fd, page2kva(page) + off, size, offset)) != 0)
            {
                goto bad_cleanup_mmap;
            }
            start += size; offset += size;
        }
        end = ph->p_va + ph->p_memsz;

        if (start < la)
        {
            /* ph->p_memsz == ph->p_filesz */
            if (start == end)
            {
                continue ;
            }
            off = start + PGSIZE - la; size = PGSIZE - off;
            if (end < la)
            {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
            assert((end < la && start == end) || (end >= la && start == la));
        }
        
        while (start < end)
        {
            if ((page = pgdir_alloc_page(mm, la, perm)) == NULL)
            {
                ret = -E_NO_MEM;
                goto bad_cleanup_mmap;
            }
            off = start - la; size = PGSIZE - off; la += PGSIZE;
            if (end < la)
            {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
        }
    }
    
    // elf 程序已经全部加载到内存，可以关闭文件了
    sysfile_close(fd);

    // 映射用户进程堆栈地址空间
    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0)
    {
        goto bad_cleanup_mmap;
    }
    
    // 创建页表，这里先只创建 4 个页面的页表，这里要权限是 PTE_USER 用户权限，否则用户态无法访问堆栈
    assert(pgdir_alloc_page(mm, USTACKTOP - PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm, USTACKTOP - 2 * PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm, USTACKTOP - 3 * PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm, USTACKTOP - 4 * PGSIZE , PTE_USER) != NULL);
    
    mm_count_inc(mm);
    current->mm = mm;
    current->cr3 = PADDR(mm->pgdir);
    lcr3(PADDR(mm->pgdir));

    //setup argc, argv
    uint32_t argv_size = 0, i;
    for (i = 0; i < argc; i++)
    {
        argv_size += strnlen(kargv[i], EXEC_MAX_ARG_LEN + 1) + 1;
    }

    // 用户进程的用户态堆栈地址，是根据虚拟空间地址规划出来的
    // 栈空间只是在虚拟地址连续，物理地址未必连续，都是先分配的虚拟页面，然后写的时候触发写异常，
    // 然后 page_fault 之后分配物理内存，这种做法避免一开始分配内存又不是实际使用，造成浪费
    // 当然内核栈的那两页是连续的物理页面
    uintptr_t stacktop = USTACKTOP - (argv_size / sizeof(long) + 1) * sizeof(long);
    char **uargv = (char **)(stacktop  - argc * sizeof(char *));
    
    argv_size = 0;
    for (i = 0; i < argc; i ++)
    {
        uargv[i] = strcpy((char *)(stacktop + argv_size), kargv[i]);
        argv_size += strnlen(kargv[i], EXEC_MAX_ARG_LEN + 1) + 1;
    }
    
    // 把程序启动的参数先放到堆栈里，这样用户进程通过 main 函数启动之后可以获取到 argc，argv
    stacktop = (uintptr_t)uargv - sizeof(int);
    *(int *)stacktop = argc;
    
    // 用户进程是通过 sys_exec 系统调用作为入口进来加载的，实际也是中断，中断返回就需要构造中断帧
    // 这里中断桢设置的是 USER_CS 和 USER_DS，所以进程运行起来后直接是用户态
    struct trapframe *tf = current->tf;
    memset(tf, 0, sizeof(struct trapframe));
    tf->tf_cs = USER_CS;
    tf->tf_ds = tf->tf_es = tf->tf_ss = USER_DS;
    tf->tf_esp = stacktop; // 用户态堆栈地址
    tf->tf_eip = elf->e_entry;  // e_entry 实际是 initcode.S 里的 _start
    tf->tf_eflags = FL_IF;
    ret = 0;
out:
    return ret;
bad_cleanup_mmap:
    exit_mmap(mm);
bad_elf_cleanup_pgdir:
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    goto out;
}

// this function isn't very correct
static void put_kargv(int argc, char **kargv)
{
    while (argc > 0)
    {
        kfree(kargv[-- argc]);
    }
}

static int copy_kargv(struct mm_struct *mm, int argc, char **kargv, const char **argv)
{
    int i, ret = -E_INVAL;
    if (!user_mem_check(mm, (uintptr_t)argv, sizeof(const char *) * argc, 0))
    {
        return ret;
    }
    for (i = 0; i < argc; i ++)
    {
        char *buffer;
        if ((buffer = kmalloc(EXEC_MAX_ARG_LEN + 1)) == NULL)
        {
            goto failed_nomem;
        }
        if (!copy_string(mm, buffer, argv[i], EXEC_MAX_ARG_LEN + 1))
        {
            kfree(buffer);
            goto failed_cleanup;
        }
        kargv[i] = buffer;
    }
    return 0;

failed_nomem:
    ret = -E_NO_MEM;
failed_cleanup:
    put_kargv(i, kargv);
    return ret;
}

// do_execve - call exit_mmap(mm)&put_pgdir(mm) to reclaim memory space of current process
//           - call load_icode to setup new memory space accroding binary prog.
/*
 当进程调用一种 exec 函数时，源进程完全由新程序代换，而新程序则从其 main 函数开始执行。
 因为调用 exec 并不创建新进程，所以前后的进程 ID 并未改变。
 exec 只是用另一个新程序替换了当前进程的正文、数据、堆和栈段。特别地，在原进程中已经打开的文件描述符，
*/
int do_execve(const char *name, int argc, const char **argv)
{
    static_assert(EXEC_MAX_ARG_LEN >= FS_MAX_FPATH_LEN);
    struct mm_struct *mm = current->mm;
    if (!(argc >= 1 && argc <= EXEC_MAX_ARG_NUM))
    {
        return -E_INVAL;
    }

    char local_name[PROC_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));
    
    char *kargv[EXEC_MAX_ARG_NUM];
    const char *path;
    
    int ret = -E_INVAL;
    
    lock_mm(mm);
    if (name == NULL)
    {
        snprintf(local_name, sizeof(local_name), "<null> %d", current->pid);
    }
    else
    {
        // 这里要 copy_string，不能直接用 name 这个变量，是因为当前是内核态，name 来自用户空间
        if (!copy_string(mm, local_name, name, sizeof(local_name)))
        {
            unlock_mm(mm);
            return ret;
        }
    }
    
    // 这里要 copy_kargv，不能直接用 argv 这个变量，是因为当前是内核态，name 来自用户空间
    if ((ret = copy_kargv(mm, argc, kargv, argv)) != 0)
    {
        unlock_mm(mm);
        return ret;
    }
    path = argv[0];
    unlock_mm(mm);
    
    // 父进程在 fork 之前，关联的文件子进程都能继承，父子进程指向同样的文件节点，只是文件节点的引用次数增加
    // 但这里因为要执行 execve，运行新的用户程序，所以先把从父进程那里继承到的文件全部关闭（除 stdin，stdout）
    files_closeall(current->filesp);

    /* sysfile_open will check the first argument path, thus we have to use a user-space pointer, and argv[0] may be incorrect */    
    int fd;
    if ((ret = fd = sysfile_open(path, O_RDONLY)) < 0)
    {
        goto execve_exit;
    }

    // 采用 excecv 加载程序时，是先创建的进程，后加载的应用程序二进制文件
    // 所以这里先释放在 fork 创建进程时，从父进程那里继承的页面资源，页表资源
    // 等后续把程序从磁盘上读到内存里之后，再重新构建页表结构
    if (mm != NULL)
    {
        lcr3(boot_cr3);
        if (mm_count_dec(mm) == 0)
        {
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);
        }
        current->mm = NULL;
    }
    ret = -E_NO_MEM;
    if ((ret = load_icode(fd, argc, kargv)) != 0)
    {
        goto execve_exit;
    }
    
    // 下面用到的参数 kargv，local_name，不能直接用函数入口参数 name，argv，因为代码走到这里，
    // load_icode 函数里已经重新设置了用户进程相关 mm 页表，用户进程页表里 name，argv 是非法地址
    put_kargv(argc, kargv);
    set_proc_name(current, local_name);
    cprintf("do_execve: name = \"%s\", kstack = %x.\n", local_name, current->kstack);
    print_trapframe(current->tf);
    return 0;

execve_exit:
    put_kargv(argc, kargv);
    do_exit(ret);
    panic("already exit: %e.\n", ret);
}

// do_yield - ask the scheduler to reschedule
int do_yield(void)
{
    cprintf("do_yield: pid = %d, name = \"%s\".\n", current->pid, current->name);
    current->need_resched = 1;
    return 0;
}

// do_wait - wait one OR any children with PROC_ZOMBIE state, and free memory space of kernel stack
//         - proc struct of this child.
// NOTE: only after do_wait function, all resources of the child proces are free.
int do_wait(int pid, int *code_store)
{
    struct mm_struct *mm = current->mm;
    if (code_store != NULL)
    {
        if (!user_mem_check(mm, (uintptr_t)code_store, sizeof(int), 1))
        {
            return -E_INVAL;
        }
    }

    struct proc_struct *proc;
    bool intr_flag, haskid;
repeat:
    haskid = 0;
    if (pid != 0)
    {
        proc = find_proc(pid);
        if (proc != NULL && proc->parent == current)
        {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE)
            {
                goto found;
            }
        }
    }
    else
    {
        // pid = 0 一般只有 init 进程会这样使用
        // 这表示去遍历自己所管理的所有子进程，看有没有退出的
        proc = current->cptr;
        for (; proc != NULL; proc = proc->optr)
        {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE)
            {
                goto found;
            }
        }
    }
    if (haskid)
    {
        current->state = PROC_SLEEPING;
        // 记录进程处于等待的原因
        current->wait_state = WT_CHILD;
        schedule();
        // 检测当前进程是否被 kill 过
        if (current->flags & PF_EXITING)
        {
            do_exit(-E_KILLED);
        }
        goto repeat;
    }
    return -E_BAD_PROC;

found:
    if (proc == idleproc || proc == initproc)
    {
        panic("wait idleproc or initproc.\n");
    }
    if (code_store != NULL)
    {
        *code_store = proc->exit_code;
    }
    local_intr_save(intr_flag);
    {
        unhash_proc(proc);
        remove_links(proc);
    }
    local_intr_restore(intr_flag);
    put_kstack(proc);
    kfree(proc);
    return 0;
}

// do_kill - kill process with pid by set this process's flags with PF_EXITING
int do_kill(int pid)
{
    struct proc_struct *proc;
    if ((proc = find_proc(pid)) != NULL)
    {
        if (!(proc->flags & PF_EXITING))
        {
            // 标记当前进程处于被 kill 的状态，这里只是标记状态，并非直接干掉进程，等进程自己结束自己
            proc->flags |= PF_EXITING;
            // 如果进程处于可中断的等待状态中，则进行唤醒，如果是处于 sem 信号量等待，则不唤醒
            if (proc->wait_state & WT_INTERRUPTED)
            {
                wakeup_proc(proc);
            }
            return 0;
        }
        return -E_KILLED;
    }
    return -E_INVAL;
}

// kernel_execve - do SYS_exec syscall to exec a user program called by user_main kernel_thread
// 通过系统调动来执行用户进程
static int kernel_execve(const char *name, const char **argv)
{
    int argc = 0, ret;
    while (argv[argc] != NULL)
    {
        argc++;
    }
    asm volatile (
        "int %1;"
        : "=a" (ret)
        : "i" (T_SYSCALL), "0" (SYS_exec), "d" (name), "c" (argc), "b" (argv)
        : "memory");
    return ret;
}

#define __KERNEL_EXECVE(name, path, ...) ({                         \
const char *argv[] = {path, ##__VA_ARGS__, NULL};       \
                     cprintf("kernel_execve: pid = %d, name = \"%s\".\n",    \
                             current->pid, name);                            \
                     kernel_execve(name, argv);                              \
})

#define KERNEL_EXECVE(x, ...)                   __KERNEL_EXECVE(#x, #x, ##__VA_ARGS__)

#define KERNEL_EXECVE2(x, ...)                  KERNEL_EXECVE(x, ##__VA_ARGS__)

#define __KERNEL_EXECVE3(x, s, ...)             KERNEL_EXECVE(x, #s, ##__VA_ARGS__)

#define KERNEL_EXECVE3(x, s, ...)               __KERNEL_EXECVE3(x, s, ##__VA_ARGS__)

// user_main - kernel thread used to exec a user program
// 通过内核线程来执行一个用户程序，并切换到用户态，这里是直接用 user_main 来执行 sh，
// 这里是先创建了 proc task（user_main）然后再去加载执行 sh 代码文件，
// 并没有 fork 新的进程去执行 sh，sh 就是 user_main 本身
static int user_main(void *arg)
{
#ifdef TEST
#ifdef TESTSCRIPT
    KERNEL_EXECVE3(TEST, TESTSCRIPT);
#else
    KERNEL_EXECVE2(TEST);
#endif
#else
//    KERNEL_EXECVE(sh);
    char *name = "sh";
    const char *argv[] = {name, NULL};
    cprintf("user_main execve: pid = %d, name = \"%s\".\n", current->pid, name);
    kernel_execve(name, argv);
#endif
    panic("user_main execve failed.\n");
}

// init_main - the second kernel thread used to create user_main kernel threads
static int init_main(void *arg)
{
    int ret = 0;
    // initproc 分配了文件系统资源和文件系统
    // 设置 initproc 进程当前目录为根目录 disk0 对应的 inode 节点
    if ((ret = vfs_set_bootfs("disk0:")) != 0)
    {
        panic("set boot fs failed: %e.\n", ret);
    }
    
    size_t nr_free_pages_store = nr_free_pages();
    size_t kernel_allocated_store = kallocated();

    // 通过 user_main 内核线程来启动用户进程（shell），并切换到用户态
    int pid = kernel_thread(user_main, NULL, 0, "user_main");
    if (pid <= 0)
    {
        panic("create user_main failed.\n");
    }
    
    struct proc_struct *userproc = find_proc(pid);
    set_proc_name(userproc, "user_main");
    
    extern void check_sync(void);
//    check_sync();                // check philosopher sync problem

    // init 在启动 shell 之后，就进入休眠等待状态了，不再进行调度，专门负责清理用户进程退出后的资源
    // 后续由 idle 负责持续调度进程
    while (do_wait(0, NULL) == 0)
    {
        schedule();
    }

    fs_cleanup();
        
    cprintf("all user-mode processes have quit.\n");
    assert(initproc->cptr == NULL && initproc->yptr == NULL && initproc->optr == NULL);
    assert(nr_process == 2);
    assert(list_next(&proc_list) == &(initproc->list_link));
    assert(list_prev(&proc_list) == &(initproc->list_link));
    assert(nr_free_pages_store == nr_free_pages());
    assert(kernel_allocated_store == kallocated());
    cprintf("init check memory pass.\n");
    return 0;
}

// proc_init - set up the first kernel thread idleproc "idle" by itself and 
//           - create the second kernel thread init_main
void proc_init(void)
{
    int i;

    list_init(&proc_list);
    for (i = 0; i < HASH_LIST_SIZE; i ++)
    {
        list_init(hash_list + i);
    }

    if ((idleproc = alloc_proc()) == NULL)
    {
        panic("cannot alloc idleproc.\n");
    }

    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    
    // 只有 idleproc 使用最早创建的内核栈，后续无论再创建别的内核线程，还是用户进程，都是重新分配的内核栈空间
    extern char bootstack[];
    idleproc->kstack = (uintptr_t)bootstack;    //0xC0152000
    idleproc->need_resched = 1;
    
    // idleproc 只分配了文件系统资源，但没有关联实际的文件系统
    if ((idleproc->filesp = files_create()) == NULL)
    {
        panic("create filesp (idleproc) failed.\n");
    }
    files_count_inc(idleproc->filesp);
    
    set_proc_name(idleproc, "idle");
    nr_process++;

    current = idleproc;

    int pid = kernel_thread(init_main, NULL, 0, "init_main");
    if (pid <= 0)
    {
        panic("create init_main failed.\n");
    }

    initproc = find_proc(pid);
    set_proc_name(initproc, "init");

    assert(idleproc != NULL && idleproc->pid == 0);
    assert(initproc != NULL && initproc->pid == 1);
}

// cpu_idle - at the end of kern_init, the first kernel thread idleproc will do below works
// idle 进程创建之后只干一件事，就是 schedule 调度进程
void cpu_idle(void)
{
    while (1)
    {
        if (current->need_resched)
        {
            schedule();
        }
    }
}

// set the process's priority (bigger value will get more CPU time) 
void set_priority(uint32_t priority)
{
    if (priority == 0)
        current->priority = 1;
    else
        current->priority = priority;
}

// do_sleep - set current process state to sleep and add timer with "time"
//          - then call scheduler. if process run again, delete timer first.
int do_sleep(unsigned int time)
{
    if (time == 0)
    {
        return 0;
    }
    bool intr_flag;
    local_intr_save(intr_flag);
    timer_t __timer, *timer = timer_init(&__timer, current, time);
    current->state = PROC_SLEEPING;
    // 记录进程处于等待的原因
    current->wait_state = WT_TIMER;
    add_timer(timer);
    local_intr_restore(intr_flag);

    schedule();

    del_timer(timer);
    return 0;
}

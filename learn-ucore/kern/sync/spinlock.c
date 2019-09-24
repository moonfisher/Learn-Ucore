// Mutual exclusion spin locks.

#include "defs.h"
#include "assert.h"
#include "x86.h"
#include "memlayout.h"
#include "string.h"
#include "cpu.h"
#include "spinlock.h"
#include "kdebug.h"
#include "stdio.h"
#include "preempt.h"
#include "intr.h"
#include "sync.h"

/*
 自旋锁的初衷：
 在短期间内进行轻量级的锁定。一个被争用的自旋锁使得请求它的线程在等待锁重新可用的期间，进行自旋
 (特别浪费处理器时间)，所以自旋锁不应该被持有时间过长。如果需要长时间锁定的话，最好使用信号量。

 单处理器的自旋锁：
 1）如果在系统不支持内核抢占时，自旋锁的实现也是空的，因为单核只有一个线程在执行，不会有内核抢占，
 从而资源也不会被其他线程访问到。
 2）如果系统支持内核抢占，由于自旋锁是禁止抢占内核的，所以不会有其他的进程因为等待锁而自旋.
 3）只有在多 cpu 下，其他的 cpu 因为等待该 cpu 释放锁，而处于自旋状态，不停轮询锁的状态。
    所以这样的话，如果一旦自旋锁内代码执行时间较长，等待该锁的 cpu 会耗费大量资源，也是不同于
    信号量和互斥锁的地方。
 
 简单来说，自旋锁在内核中主要用来防止多处理器中并发访问临界区，防止内核抢占造成的竞争。

 自旋锁内睡眠禁止睡眠问题：
 如果自旋锁锁住以后进入睡眠，而此时又不能进行处理器抢占(锁住会disable prempt)，其他进程无法获得
 cpu，这样也不能唤醒睡眠的自旋锁，因此不响应任何操作。

 自旋锁为什么广泛用于内核：
 自旋锁是一种轻量级的互斥锁，可以更高效的对互斥资源进行保护。自旋锁本来就只是一个很简单的同步机制，
 在 SMP 之前根本就没这个东西，一切都是 Event 之类的同步机制，这类同步机制都有一个共性就是：一旦
 资源被占用都会产生任务切换，任务切换涉及很多东西的(保存原来的上下文，按调度算法选择新的任务，恢复
 新任务的上下文，还有就是要修改 cr3 寄存器会导致 cache 失效)这些都是需要大量时间的，因此用 Event
 之类来同步一旦涉及到阻塞代价是十分昂贵的，而自旋锁的效率就远高于互斥锁。
*/

/*
 什么时候用 spin_lock，什么时候用 spin_lock_irq ？
 
 spin_lock 比 spin_lock_irq 速度快，但是它并不是任何情况下都是安全的。
 举个例子:进程 A 中调用了 spin_lock（&lock）然后进入临界区，此时来了一个中断(interrupt），
 该中断也运行在和进程 A 相同的 CPU 上，并且在该中断处理程序中恰巧也会 spin_lock(&lock)
 试图获取同一个锁。由于是在同一个 CPU 上被中断，进程 A 会被设置为 TASK_INTERRUPT 状态，
 中断处理程序无法获得锁，会不停的忙等，中断程序由于进程 A 被设置为中断状态，schedule（）进程调度
 就无法再调度进程 A 运行，这样就导致了死锁！
 但是如果该中断处理程序运行在不同的 CPU 上就不会触发死锁。 因为在不同的 CPU 上出现中断不会导致
 进程 A 的状态被设为 TASK_INTERRUPT，只是换出。当中断处理程序忙等被换出后，进程 A 还是有机会
 获得 CPU，执行并退出临界区。
*/

// The big kernel lock
struct spinlock kernel_lock = {
	.name = "kernel_lock"
};

// Record the current call stack in pcs[] by following the %ebp chain.
static void get_caller_pcs(uint32_t pcs[])
{
	uint32_t *ebp;
	int i;

	ebp = (uint32_t *)read_ebp();
	for (i = 0; i < 10; i++)
	{
		if (ebp == 0 || ebp < (uint32_t *)KERNBASE)
			break;
		pcs[i] = ebp[1];		  // saved %eip
		ebp = (uint32_t *)ebp[0]; // saved %ebp
	}
	for (; i < 10; i++)
		pcs[i] = 0;
}

// Check whether this CPU is holding the lock.
static int holding(struct spinlock *lock)
{
	return lock->locked && lock->cpu == thiscpu;
}

void __spin_initlock(struct spinlock *lk, char *name)
{
	lk->locked = 0;
	lk->name = name;
	lk->cpu = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// Holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
void spin_acquire(struct spinlock *lk)
{
    if (holding(lk))
    {
        cprintf("CPU %d cannot acquire %s: already holding", cpunum(), lk->name);
        return;
    }
    
    // The xchg is atomic.
    // It also serializes, so that reads after acquire are not
    // reordered before it.
    // 自旋锁，为了减少 task 切换带来的开销，这里采用循环检测的方式来判断锁是否释放
    // 采用自旋锁的场景，临界区的代码执行速度要快，业务要少，不能太复杂太耗时，否则自旋锁没有优势
    /*
     xchg 指令就同时做到了交换 locked 和 1 的值，并且在之后通过检查 eax 寄存器就能知道 locked
     的值是否为 0。 并且，以上操作是原子的，这就保证了有且只有一个进程能够拿到 locked 的 0 值
     并且进入临界区。
     */
    while (xchg(&lk->locked, 1) != 0) //原理见：https://pdos.csail.mit.edu/6.828/2018/xv6/book-rev11.pdf  chapter 4
        asm volatile("pause");

        // Record info about lock acquisition for debugging.
    lk->cpu = thiscpu;
    get_caller_pcs(lk->pcs);
}

// Release the lock.
void spin_release(struct spinlock *lk)
{
    if (!holding(lk))
    {
        int i;
        uint32_t pcs[10];
        // Nab the acquiring EIP chain before it gets released
        memmove(pcs, lk->pcs, sizeof pcs);
        cprintf("CPU %d cannot release %s: held by CPU %d\nAcquired at:", cpunum(), lk->name, lk->cpu->cpu_id);
        for (i = 0; i < 10 && pcs[i]; i++)
        {
//            struct Eipdebuginfo info;
//            if (debuginfo_eip(pcs[i], &info) >= 0)
//                cprintf("  %08x %s:%d: %.*s+%x\n", pcs[i],
//                        info.eip_file, info.eip_line,
//                        info.eip_fn_namelen, info.eip_fn_name,
//                        pcs[i] - info.eip_fn_addr);
//            else
//                cprintf("  %08x\n", pcs[i]);
        }
        panic("spin_unlock");
    }

    lk->pcs[0] = 0;
    lk->cpu = 0;
    
    // The xchg instruction is atomic (i.e. uses the "lock" prefix) with
    // respect to any other instruction which references the same memory.
    // x86 CPUs will not reorder loads/stores across locked instructions
    // (vol 3, 8.2.2). Because xchg() is implemented using asm volatile,
    // gcc will not reorder C statements across the xchg.
    xchg(&lk->locked, 0);
}

/*
 1）spin_lock 只禁止内核抢占，不会关闭本地中断，比 spin_lock_irq 速度快
 2）为何需要关闭内核抢占：假如进程 A 获得 spin_lock -> 进程 B 抢占进程 A -> 进程 B 尝试获取
    spin_lock -> 由于进程 B 优先级比进程 A 高，先于 A 运行，而进程 B 又需要 A unlock 才得
    以运行，这样死锁。所以这里需要关闭抢占
 3) 没关中断带来的问题：假如进程 A 获得 spin_lock -> 此时发生中断进入中断函数 -> 中断函数尝试
    获取 spin_lock -> 中断函数无法获取成功，自旋等待 -> 中断函数无法退出，进程 A 也不可能再次
    调度运行，A 无法释放 spin_lock -> 导致死锁。所以如果临界区和中断函数都会使用同一个自旋锁，
    这种场景下需要使用 spin_lock_irq
 */
void spin_lock(struct spinlock *lk)
{
    // 关闭抢占，防止死锁
    preempt_disable();
    spin_acquire(lk);
}

void spin_trylock(struct spinlock *lk)
{
    
}

void spin_unlock(struct spinlock *lk)
{
    spin_release(lk);
    // 解锁的时候，是一个抢占点
    preempt_enable();
}

/*
 1）禁止内核抢占，且关闭本地中断
 2）那么在 spin_lock 中关闭了内核抢占，不关闭中断会出现什么情况呢？
    假如中断中也想获得这个锁，会出现和 spin_lock 中举得例子相同。所以这个时候，
    在进程 A 获取 lock 之后，使用 spin_lock_irq 将中断禁止，就不会出现死锁的情况
 3）在任何情况下使用 spin_lock_irq 都是安全的。因为它既禁止本地中断，又禁止内核抢占。
 4）spin_lock 比 spin_lock_irq 速度快，但是它并不是任何情况下都是安全的。
 
 在关闭本地中断后是否有必要关闭抢占？
 
 关中断之后，任何外部事件都不能打扰 CPU 连续执行临界区程序。如果临界区程序本身不去主动调度别的程序，
 那么这种方法就能保证临界区作为一个整体执行。这种方法的优点是简单、可靠，但也有一定的局限性。

 1) 它不能用于多 CPU 系统。其原因是，由于该系统中的多个 CPU 都有其各自的中断开关，因此一个 CPU
 并不能阻止在其它 CPU 上运行的进程进入同类临界区。
 2) 在临界区中如果有代码能唤醒其它进程去运行，则也不能使用这种方法。因为在该进程进入封锁状态后，系
 统将调度另一进程使用 CPU ，如果需要，该进程也可以执行临界区程序，不会受到任何阻
 拦，所以在这种情况下，开、关中断不能实施临界区互斥。
 3) 如果临界区代码执行时间比较长，则本法会降低中断响应速度。
 4) 这是一把锁处理各类临界区，不必要地扩大了互斥范围。

 在 spin_lock_irq 函数，也就是在自旋锁中关闭中的这类函数中，既然已经关闭了本地中断，再禁止抢占
 有没有多余。也就是说，既然本地中断已经禁止了，在本处理器上是无法被打断的，本地调度器也无法运行，
 也就不可以被本地调度程序调度出去...

 从 spinlock 设计原理看，使用它的时候，在临界区间是务必确保不会发生进程切换。现在的问题是，如果
 已经关闭了中断，在同一处理器上如果不关掉内核抢占的特性，会不会有进程调度的情况发生，如果没有，那在
 local_irq_disable 之后再使用 peempt_disable 就多此一举了?

 这个在 SMP 系统上最好理解了，假设有 CPU A 和 CPU B 两个处理器，使用 spin lock 的进程 X 运行
 在 CPU A 上，一种很明显的情形就是如果有个进程 Y 先于 X 运行，但是因为 Y 等待网卡的一个数据包，
 它进入了 sleep 状态，然后 X 开始被调度运行，然后 X 在 spin lock 获得锁后进入临界区，此时网卡收
 到 Y 的数据包，因为 X 只是关闭了 CPU A 上的中断，所以 CPU B 还是会接收并处理该中断，然后唤醒 Y，
 Y 进入运行队列，此时出现一个调度点，如果 Y 的优先级高于 X ，那么就有进程切换发生了，但是如果 X
 所使用的 spin lock 中关闭了内核抢占，那么就使得进程切换成为不可能。
 
 如果是在单处理器系统上，local_irq_disable 实际上关闭了所有（其实就一个）处理器的中断，
 所有有中断引起的调度点都不可能存在，此时有无其他与中断无关的调度点出现呢？在 2.4 上，因为没有抢占，
 这种情形绝无可能，事实上，早期的内核很大程度上是依赖 local_irq_disable 来做资源保护。

 2.6 有了抢占的概念，local_irq_save 等函数只是禁止了本地中断，即当前 CPU 上的中断。
 在单核 CPU 上，当然抢占就不可能发生了，但是在多核 CPU 上由于其他核上的中断并没有被禁止，
 是仍然可能发生抢占的，但本 CPU 内不会被抢占。
*/
void spin_lock_irq(struct spinlock *lk)
{
    local_irq_disable();
    preempt_disable();
    spin_acquire(lk);
}

void spin_trylock_irq(struct spinlock *lk)
{
    
}

void spin_unlock_irq(struct spinlock *lk)
{
    spin_release(lk);
    local_irq_enable();
    preempt_enable();
}

/*
 1) 禁止内核抢占，关闭中断，保存中断状态寄存器的标志位
 2) spin_lock_irqsave 在锁返回时，之前开的中断，之后也是开的；之前关，之后也是关。
    但是 spin_lock_irq 则不管之前的开还是关，返回时都是开的
 3) spin_lock_irq 在自旋的时候，不会保存当前的中断标志寄存器，只会在自旋结束后，将之前的中断打开。
 
 使用 spin_lock_irqsave 在于你不期望在离开临界区后，改变中断的开启/关闭状态！
 进入临界区是关闭的，离开后它同样应该是关闭的！
*/
int spin_lock_irqsave(struct spinlock *lk)
{
    int flags;
    
    local_intr_save(flags);
    preempt_disable();
    spin_acquire(lk);

    return flags;
}

void spin_trylock_irqsave(struct spinlock *lk)
{
    
}

void spin_unlock_irqrestore(struct spinlock *lk, int flags)
{
    spin_release(lk);
    local_intr_restore(flags);
    preempt_enable();
}

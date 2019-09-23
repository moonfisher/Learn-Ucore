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
 在真正的上锁前，为何要调用 preempt_disable() 来关闭抢占？

 如果内核可抢占, 单 CPU process 1 通过系统调用进入内核态，如果其需要访问临界区，
 则在进入临界区前获得锁，上锁，V=1，然后进入临界区, 如果 process 1  在内核态执行临界区代码
 的过程中发生了一个外部中断，当中断处理函数返回时，因为内核的可抢占性，此时将会出现一个调度点，
 如果 CPU 的运行队列中出现了一个比当前被中断进程 process 1 优先级更高的进程 process 2，
 那么被中断的进程将会被换出处理器，即便此时它正运行于内核态。
 如果 process 2  也通过系统调用进入内核态，且要访问相同的临界区，则会形成死锁(因为拥有锁的
 Process 1 永没有机会再运行从而释放锁）

 为了防止系统进入死锁状态，需要在真正上锁前，调用 preempt_disable() 来关闭抢占
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

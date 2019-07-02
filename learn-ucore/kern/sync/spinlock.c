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
void spin_lock(struct spinlock *lk)
{
	if (holding(lk))
    {
		cprintf("CPU %d cannot acquire %s: already holding", cpunum(), lk->name);
        return;
    }
    
	// The xchg is atomic.
	// It also serializes, so that reads after acquire are not
	// reordered before it.
	while (xchg(&lk->locked, 1) != 0) //原理见：https://pdos.csail.mit.edu/6.828/2018/xv6/book-rev11.pdf  chapter 4
		asm volatile("pause");

		// Record info about lock acquisition for debugging.
	lk->cpu = thiscpu;
	get_caller_pcs(lk->pcs);
}

// Release the lock.
void spin_unlock(struct spinlock *lk)
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

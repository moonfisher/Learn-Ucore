#ifndef _SPINLOCK_H
#define _SPINLOCK_H

#include "defs.h"

// Mutual exclusion lock.
struct spinlock
{
	unsigned locked;        // Is the lock held?
	char *name;			    // Name of lock.
	struct cpu_info *cpu;   // The CPU holding the lock.
	uintptr_t pcs[10];      // The call stack (an array of program counters)
                            // that locked the lock.
};

void __spin_initlock(struct spinlock *lk, char *name);

void spin_lock(struct spinlock *lk);
void spin_trylock(struct spinlock *lk);
void spin_unlock(struct spinlock *lk);

void spin_lock_irq(struct spinlock *lk);
void spin_trylock_irq(struct spinlock *lk);
void spin_unlock_irq(struct spinlock *lk);

int spin_lock_irqsave(struct spinlock *lk);
void spin_trylock_irqsave(struct spinlock *lk);
void spin_unlock_irqrestore(struct spinlock *lk, int flags);

#define spin_initlock(lock) __spin_initlock(lock, #lock)

extern struct spinlock kernel_lock;

static inline void lock_kernel(void)
{
	spin_lock(&kernel_lock);
}

static inline void unlock_kernel(void)
{
	spin_unlock(&kernel_lock);

	// Normally we wouldn't need to do this, but QEMU only runs
	// one CPU at a time and has a long time-slice.  Without the
	// pause, this CPU is likely to reacquire the lock before
	// another CPU has even been given a chance to acquire it.
	asm volatile("pause");
}

#endif

#ifndef __KERN_SYNC_SYNC_H__
#define __KERN_SYNC_SYNC_H__

#include "x86.h"
#include "intr.h"
#include "mmu.h"
#include "assert.h"
#include "atomic.h"
#include "sched.h"

static inline bool irqs_disabled(void)
{
    if (read_eflags() & FL_IF)
    {
        return 1;
    }
    return 0;
}

// 保存本地中断传递的当前状态，然后禁止本地中断传递
static inline bool __intr_save(void)
{
    if (read_eflags() & FL_IF)
    {
        local_irq_disable();
        return 1;
    }
    return 0;
}

// 恢复本地中断传递到给定的状态
static inline void __intr_restore(bool flag)
{
    if (flag)
    {
        local_irq_enable();
    }
}

// 保存本地中断传递的当前状态，然后禁止本地中断传递
#define local_intr_save(x) \
    do                     \
    {                      \
        x = __intr_save(); \
    } while (0)

// 恢复本地中断传递到给定的状态
#define local_intr_restore(x)   __intr_restore(x);

void sync_init(void);

#endif /* !__KERN_SYNC_SYNC_H__ */


#include "preempt.h"
#include "proc.h"
#include "x86.h"
#include "sync.h"
#include "sched.h"

void preempt_count_inc(void)
{
    current->preempt_count++;
}

void preempt_count_dec(void)
{
    current->preempt_count--;
}

int preempt_count(void)
{
    return current->preempt_count;
}

bool preemptible(void)
{
    return (preempt_count() == 0 && !irqs_disabled());
}

void preempt_schedule(void)
{
    /*
     * If there is a non-zero preempt_count or interrupts are disabled,
     * we do not want to preempt the current task. Just return..
     */
    if (!preemptible())
        return;

    schedule();
    /*
     * Check again in case we missed a preemption opportunity
     * between schedule and now.
     */
    barrier();
}

void preempt_disable(void)
{
    preempt_count_inc();
    barrier();
}

void preempt_enable(void)
{
    barrier();
    preempt_schedule();
}

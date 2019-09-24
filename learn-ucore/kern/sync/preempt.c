
#include "preempt.h"
#include "proc.h"
#include "x86.h"
#include "sync.h"
#include "sched.h"

/*
 为什么 linux 不是实时的。

 某场景：
 一个普通任务运行过程中，通过系统调用进入内核态拿了一把 spin_lock 这样的锁，在拿锁过程中，
 发生了硬件中断，于是 cpu 立即去处理硬件中断， 在这个硬件中断处理函数（ISR）中唤醒了一个 RT 任务，
 硬件中断处理完后，还有可能处理软中断，也有可能没有，根据设备驱动实际场景决定（中断底半步）。当中断
 顶半步 & 底半步全部执行完后，事实上 RT 任务还是得不到运行的，因为前面有一个普通任务拿了 spinlock
 锁，spinlock 是会关抢占的，所以还要等到普通任务调用到 spin_unlock 的那一刻，RT 任务才能抢占。

 由上面例子可以看出，从唤醒 RT 任务到 RT 任务被执行，这段时间是不可预期的，所以通常 linux 不是
 一个硬实时的系统。
 
 如何让 linux 变成一个硬实时操作系统呢？首先要知道什么是实时操作系统，实时操作系统的重要特性就是
 系统中的实时任务，要在一个可预期的时间范围内必须得到执行。当一个高优先级任务被唤醒执行，或主动执行时，
 他必须可以立即抢占其他任务，得到 cpu 的执行权，这段时间必须是可预期的。像我们所熟知的 vxworks 实时
 系统，可以做到 10ns 以内可预期。
 
 linux 内核属于宏内核，和 vxworks 微内核设计思想不一样，linux 大量用在服务器、嵌入式领域。
 服务器更追求的是高密度计算，系统吞吐能力。很多产品、工程场景，并不要求有多么精准的实时性。
 
 补丁原理：
 spin_lock 锁会关掉 cpu 抢占调度，影响实时性。所以 RT 补丁将 spin_lock 锁变成可以抢占了，
 这样就不用等到 unlock 时才能调度到 rt 任务。

 RT 补丁打入内核后，内核额外提供了几种抢占模型：
 
 1) No Forced Preemption (Server)               ----不强制抢占
 2) Voluntary Kernel Preemption (Desktop)       ----自愿抢占
 3) Preemptible Kernel (Low-Latency Desktop)    ----抢占式内核
 4) Preemptible Kernel (Basic RT)               ----基本实时
 5) Fully Preemptible Kernel (RT)               ----完全实时

 实时性依次增强，当我们配上 Fully Preemptible Kernel 时，内核代码中所有的 mutex 锁，sem
 信号量，spin_lock 全部变成了实时锁，实时锁意味着任何地方都可以抢占，所以补丁后实时性就非常强了。
 但我们要知道实时性越强，吞吐能力就越弱。cpu 频繁任务调度的开销也是不小！
*/

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

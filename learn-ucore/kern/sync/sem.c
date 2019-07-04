#include "defs.h"
#include "wait.h"
#include "atomic.h"
#include "slab.h"
#include "sem.h"
#include "proc.h"
#include "sync.h"
#include "assert.h"

void sem_init(semaphore_t *sem, int value)
{
    sem->value = value;
    wait_queue_init(&(sem->wait_queue));
}

static __noinline void __up(semaphore_t *sem, uint32_t wait_state)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        wait_t *wait;
        // 如果等待队列里没有进程在等待信号量，信号量加 1
        if ((wait = wait_queue_first(&(sem->wait_queue))) == NULL)
        {
            sem->value++;
        }
        else
        {
            // 如果有进程处于信号等待状态，直接唤醒进程，这个进程相当于再次占用了信号量
            assert(wait->proc->wait_state == wait_state);
            wakeup_wait(&(sem->wait_queue), wait, wait_state, 1);
        }
    }
    local_intr_restore(intr_flag);
}

static __noinline uint32_t __down(semaphore_t *sem, uint32_t wait_state)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    if (sem->value > 0)
    {
        sem->value--;
        local_intr_restore(intr_flag);
        return 0;
    }
    
    // 把当前进程添加到等待队列，然后重新调度别的进程执行，当前进程进入睡眠态
    wait_t __wait, *wait = &__wait;
    wait_current_set(&(sem->wait_queue), wait, wait_state);
    local_intr_restore(intr_flag);

    schedule();

    local_intr_save(intr_flag);
    wait_current_del(&(sem->wait_queue), wait);
    local_intr_restore(intr_flag);

    if (wait->wakeup_flags != wait_state)
    {
        return wait->wakeup_flags;
    }
    return 0;
}

void up(semaphore_t *sem)
{
    __up(sem, WT_KSEM);
}

void down(semaphore_t *sem)
{
    uint32_t flags = __down(sem, WT_KSEM);
    assert(flags == 0);
}

// 尝试 down，这种操作不会引起进程阻塞，但也不一定能 down 成功
bool try_down(semaphore_t *sem)
{
    bool intr_flag, ret = 0;
    local_intr_save(intr_flag);
    if (sem->value > 0)
    {
        sem->value--;
        ret = 1;
    }
    local_intr_restore(intr_flag);
    return ret;
}

int sem_val(semaphore_t *sem)
{
    return sem->value;
}

uint32_t sem_wait_count(semaphore_t *sem)
{
    return wait_count(&(sem->wait_queue));
}

void wakeup_all(semaphore_t *sem)
{
    while(!wait_queue_empty( &(sem->wait_queue) ))
    {
        __up(sem, WT_SEM_ALL);
    }
}

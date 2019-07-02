#include "list.h"
#include "sync.h"
#include "proc.h"
#include "sched.h"
#include "stdio.h"
#include "assert.h"
#include "default_sched.h"
#include "spinlock.h"

static list_entry_t timer_list;

static struct sched_class *sched_class;

static struct run_queue *rq;

static inline void sched_class_enqueue(struct proc_struct *proc)
{
    if (proc != idleproc)
    {
        sched_class->enqueue(rq, proc);
    }
}

static inline void sched_class_dequeue(struct proc_struct *proc)
{
    sched_class->dequeue(rq, proc);
}

static inline struct proc_struct *sched_class_pick_next(void)
{
    return sched_class->pick_next(rq);
}

// 计算进程运行时间片
static void sched_class_proc_tick(struct proc_struct *proc)
{
    if (proc != idleproc)
    {
        sched_class->proc_tick(rq, proc);
    }
    else
    {
        proc->need_resched = 1;
    }
}

static struct run_queue __rq;

void sched_init(void)
{
    list_init(&timer_list);

    sched_class = &default_sched_class;

    rq = &__rq;
    rq->max_time_slice = 5;
    sched_class->init(rq);

    cprintf("sched class: %s\n", sched_class->name);
}

void wakeup_proc(struct proc_struct *proc)
{
//    cprintf("wakeup_proc: pid = %d, name = \"%s\".\n", proc->pid, proc->name);
    // 已经挂掉的进程不会再调度
    assert(proc->state != PROC_ZOMBIE);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        if (proc->state != PROC_RUNNABLE)
        {
            proc->state = PROC_RUNNABLE;
            proc->wait_state = 0;
            if (proc != current)
            {
                sched_class_enqueue(proc);
            }
        }
        else
        {
            warn("wakeup runnable process.\n");
        }
    }
    local_intr_restore(intr_flag);
}

/*
 触发进程重新调度的几个关键点：
 1）idle 进程空转，不停触发调度
 2）周期性的时钟中断，正是因为有时钟中断，操作系统内核才能一直有机会主动获取代码执行权，
    否则用户进程写个死循环就完了
 3）lock，sem，condition，这些信号控制，用户进程因为阻塞或者等待，触发调度
 4）读写 io 导致阻塞，比如从 stdin 读取数据，触发调度
 5）当前进程执行完 exit 退出了，触发调度
 6）父进程等待子进程 exit 退出后，触发调度
 7）进程自己 sleep 睡眠，触发调度
*/
void schedule(void)
{
    bool intr_flag;
    struct proc_struct *next;
    local_intr_save(intr_flag);
    {
        // 已经进入调度，先清零标记位，防止过度重复调度
        current->need_resched = 0;
        
        // 睡眠和死掉的线程不参与调度
        if (current->state == PROC_RUNNABLE)
        {
            sched_class_enqueue(current);
        }
        
        if ((next = sched_class_pick_next()) != NULL)
        {
            sched_class_dequeue(next);
        }
        
        // 如果在进程列表里找不到需要调度的进程（有可能进程处于睡眠或者 io 等待状态）
        // 就只能调度 idle 内核进程了
        if (next == NULL)
        {
            next = idleproc;
        }
        
        next->runs++;
        
        if (next != current)
        {
            proc_run(next);
        }
    }
    local_intr_restore(intr_flag);
}

void add_timer(timer_t *timer)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        assert(timer->expires > 0 && timer->proc != NULL);
        assert(list_empty(&(timer->timer_link)));
        list_entry_t *le = list_next(&timer_list);
        while (le != &timer_list)
        {
            timer_t *next = le2timer(le, timer_link);
            if (timer->expires < next->expires)
            {
                next->expires -= timer->expires;
                break;
            }
            timer->expires -= next->expires;
            le = list_next(le);
        }
        list_add_before(le, &(timer->timer_link));
    }
    local_intr_restore(intr_flag);
}

void del_timer(timer_t *timer)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        if (!list_empty(&(timer->timer_link)))
        {
            if (timer->expires != 0)
            {
                list_entry_t *le = list_next(&(timer->timer_link));
                if (le != &timer_list)
                {
                    timer_t *next = le2timer(le, timer_link);
                    next->expires += timer->expires;
                }
            }
            list_del_init(&(timer->timer_link));
        }
    }
    local_intr_restore(intr_flag);
}

void run_timer_list(void)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        list_entry_t *le = list_next(&timer_list);
        if (le != &timer_list)
        {
            timer_t *timer = le2timer(le, timer_link);
            assert(timer->expires != 0);
            timer->expires--;
            while (timer->expires == 0)
            {
                le = list_next(le);
                struct proc_struct *proc = timer->proc;
                if (proc->wait_state != 0)
                {
                    assert(proc->wait_state & WT_INTERRUPTED);
                }
                else
                {
                    // 受定时器唤醒的进程，wait_state 不可能是 0，应该是 WT_TIMER
                    warn("process %d's wait_state == 0.\n", proc->pid);
                }
                wakeup_proc(proc);
                del_timer(timer);
                if (le == &timer_list)
                {
                    break;
                }
                timer = le2timer(le, timer_link);
            }
        }
        
        // 通过周期性的时钟中断，计算当前正在运行中的进程时间片，时间片达到最大之后，重新调度
        sched_class_proc_tick(current);
    }
    local_intr_restore(intr_flag);
}

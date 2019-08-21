#include "defs.h"
#include "list.h"
#include "proc.h"
#include "assert.h"
#include "default_sched.h"

/*
 考察 round-robin 调度器，在假设所有进程都充分使用了其拥有的 CPU 时间资源的情况下，
 所有进程得到的 CPU 时间应该是相等的。但是有时候我们希望调度器能够更智能地为每个进程分配合理的 CPU 资源。
 假设我们为不同的进程分配不同的优先级，则我们有可能希望每个进程得到的时间资源与他们的优先级成正比关系。
 Stride 调度是基于这种想法的一个较为典型和简单的算法。
 
 除了简单易于实现以外，它还有如下的特点：
     可控性：如我们之前所希望的，可以证明 Stride Scheduling 对进程的调度次数正比于其优先级。
     确定性：在不考虑计时器事件的情况下，整个调度机制都是可预知和重现的。
 
 该算法的基本思想可以考虑如下：
     为每个 runnable 的进程设置一个当前状态 stride，表示该进程当前的调度权。
     另外定义其对应的 pass 值，表示对应进程在调度后，stride 需要进行的累加值。
     每次需要调度时，从当前 runnable 态的进程中选择 stride 最小的进程调度。
     对于获得调度的进程 P，将对应的 stride 加上其对应的步长 pass（只与进程的优先权有关系）。
     在一段固定的时间之后，回到 2.步骤，重新调度当前 stride 最小的进程。
 
 可以证明，如果令 P.pass = BigStride / P.priority 其中 P.priority 表示进程的优先权（大于 1），
 而 BigStride 表示一个预先定义的大常数，则该调度方案为每个进程分配的时间将与其优先级成正比。
*/

// 斜堆算法
#define USE_SKEW_HEAP 1

/* You should define the BigStride constant here*/
#define BIG_STRIDE    0x7FFFFFFF /* ??? */

/* The compare function for two skew_heap_node_t's and the
 * corresponding procs*/
static int proc_stride_comp_f(void *a, void *b)
{
     struct proc_struct *p = le2proc(a, run_pool);
     struct proc_struct *q = le2proc(b, run_pool);
     int32_t c = p->stride - q->stride;
     if (c > 0) return 1;
     else if (c == 0) return 0;
     else return -1;
}

/*
 * stride_init initializes the run-queue rq with correct assignment for
 * member variables, including:
 *
 *   - run_list: should be a empty list after initialization.
 *   - run_pool: NULL
 *   - proc_num: 0
 *   - max_time_slice: no need here, the variable would be assigned by the caller.
 *
 * hint: see proj13.1/libs/list.h for routines of the list structures.
 */
static void stride_init(struct run_queue *rq)
{
     list_init(&(rq->run_list));
     rq->run_pool = NULL;
     rq->proc_num = 0;
}

/*
 * stride_enqueue inserts the process ``proc'' into the run-queue
 * ``rq''. The procedure should verify/initialize the relevant members
 * of ``proc'', and then put the ``run_pool'' node into the
 * queue(since we use priority queue here). The procedure should also
 * update the meta date in ``rq'' structure.
 *
 * proc->time_slice denotes the time slices allocation for the
 * process, which should set to rq->max_time_slice.
 * 
 * hint: see proj13.1/libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static void stride_enqueue(struct run_queue *rq, struct proc_struct *proc)
{
#if USE_SKEW_HEAP
     rq->run_pool = skew_heap_insert(rq->run_pool, &(proc->run_pool), proc_stride_comp_f);
#else
     assert(list_empty(&(proc->run_link)));
     list_add_before(&(rq->run_list), &(proc->run_link));
#endif
     if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice)
     {
          proc->time_slice = rq->max_time_slice;
     }
     proc->rq = rq;
     rq->proc_num ++;
}

/*
 * stride_dequeue removes the process ``proc'' from the run-queue
 * ``rq'', the operation would be finished by the skew_heap_remove
 * operations. Remember to update the ``rq'' structure.
 *
 * hint: see proj13.1/libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static void stride_dequeue(struct run_queue *rq, struct proc_struct *proc)
{
#if USE_SKEW_HEAP
     rq->run_pool = skew_heap_remove(rq->run_pool, &(proc->run_pool), proc_stride_comp_f);
#else
     assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
     list_del_init(&(proc->run_link));
#endif
     rq->proc_num--;
}
/*
 * stride_pick_next pick the element from the ``run-queue'', with the
 * minimum value of stride, and returns the corresponding process
 * pointer. The process pointer would be calculated by macro le2proc,
 * see proj13.1/kern/process/proc.h for definition. Return NULL if
 * there is no process in the queue.
 *
 * When one proc structure is selected, remember to update the stride
 * property of the proc. (stride += BIG_STRIDE / priority)
 *
 * hint: see proj13.1/libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static struct proc_struct *stride_pick_next(struct run_queue *rq)
{
#if USE_SKEW_HEAP
     if (rq->run_pool == NULL)
         return NULL;
     struct proc_struct *p = le2proc(rq->run_pool, run_pool);
#else
     list_entry_t *le = list_next(&(rq->run_list));

     if (le == &rq->run_list)
          return NULL;
     
     struct proc_struct *p = le2proc(le, run_link);
     le = list_next(le);
     while (le != &rq->run_list)
     {
          struct proc_struct *q = le2proc(le, run_link);
          if ((int32_t)(p->stride - q->stride) > 0)
               p = q;
          le = list_next(le);
     }
#endif
     if (p->priority == 0)
          p->stride += BIG_STRIDE;
     else
         p->stride += BIG_STRIDE / p->priority;
     return p;
}

/*
 * stride_proc_tick works with the tick event of current process. You
 * should check whether the time slices for current process is
 * exhausted and update the proc struct ``proc''. proc->time_slice
 * denotes the time slices left for current
 * process. proc->need_resched is the flag variable for process
 * switching.
 */
// 通过周期性的时钟中断，计算当前正在运行中的进程时间片，时间片达到最大之后，重新调度
static void stride_proc_tick(struct run_queue *rq, struct proc_struct *proc)
{
    if (proc->time_slice > 0)
    {
        proc->time_slice--;
    }
    
    if (proc->time_slice == 0)
    {
        // 这里只是标记当前 task 需要重新调度，并不是马上去调度
        proc->need_resched = 1;
    }
}

struct sched_class default_sched_class = {
     .name = "stride_scheduler",
     .init = stride_init,
     .enqueue = stride_enqueue,
     .dequeue = stride_dequeue,
     .pick_next = stride_pick_next,
     .proc_tick = stride_proc_tick,
};


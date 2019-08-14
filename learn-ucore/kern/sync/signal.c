#include "assert.h"
#include "error.h"
#include "proc.h"
#include "slab.h"
#include "signal.h"
#include "string.h"
#include "vmm.h"
#include "sched.h"
#include "stdio.h"

#define get_si(x)   (&((x)->signal_info))

int __sig_setup_frame(int sign, struct sigaction *act, sigset_t oldset, struct trapframe *tf)
{
    warn("%s not implemented yet.", __func__);
    return -1;
}

void lock_sig(struct sighand_struct *sh)
{
	if (sh != NULL)
    {
		down(&(sh->sig_sem));
	}
}

void unlock_sig(struct sighand_struct *sh)
{
	if (sh != NULL)
    {
		up(&(sh->sig_sem));
	}
}

// remove sign from the pending queue
void remove_from_queue(int sign, struct sigpending *queue)
{
	if (!sigismember(queue->signal, sign))
		return;
	sigset_del(queue->signal, sign);
}

// calculate is there a signal in proc
void sig_recalc_pending(struct proc_struct *proc)
{
	if ((get_si(proc)->pending.signal & ~(get_si(proc)->blocked)) != 0
        || (get_si(proc)->signal-> shared_pending.signal & ~(get_si(proc)->blocked)) != 0)
    {
		proc->flags |= TIF_SIGPENDING;
	}
    else
    {
		proc->flags &= ~TIF_SIGPENDING;
	}
}

// return a signal every time, until empty then return 0
int dequeue_signal(struct proc_struct *proc)
{
	int sign = 0;
	if (get_si(proc)->pending.signal != 0)
    {
		while (++sign < 64)
        {
			if (!sigismember(get_si(proc)->blocked, sign)
                && sigismember(get_si(proc)->pending.signal, sign))
            {
				remove_from_queue(sign, &(get_si(proc)->pending));
				sig_recalc_pending(proc);
				return sign;
			}
		}
	}
	sign = 0;
	if (get_si(proc)->signal->shared_pending.signal != 0)
    {
		while (++sign < 64)
        {
			if (!sigismember(get_si(proc)->blocked, sign)
			    && sigismember(get_si(proc)->signal->shared_pending.signal, sign))
            {
				remove_from_queue(sign, &(get_si(proc)->signal->shared_pending));
				sig_recalc_pending(proc);
				return sign;
			}
		}
	}
	return 0;
}

// clean the pending queue
void flush_sigqueue(struct sigpending *queue)
{
	sigset_initwith(queue->signal, 0);
}

// is there a signal pending in proc
static inline bool signal_pending(struct proc_struct *proc)
{
	return proc->flags & TIF_SIGPENDING;
}

// create a signal_struct and init it
struct signal_struct *signal_create(void)
{
	struct signal_struct *sig = (struct signal_struct *)kmalloc(sizeof(struct signal_struct));
	if (sig != NULL)
    {
		set_signal_count(sig, 0);
		sig->curr_target = NULL;
		list_init(&(sig->shared_pending.list));
		sigset_initwith(sig->shared_pending.signal, 0);
	}
	return sig;
}

// free a signal_struct
void signal_destroy(struct signal_struct *sig)
{
	assert(sig != NULL && signal_count(sig) == 0);
	flush_sigqueue(&(sig->shared_pending));
	kfree(sig);
}

// create a sighand_struct and init it
struct sighand_struct *sighand_create(void)
{
	struct sighand_struct *sh = (struct sighand_struct *)kmalloc(sizeof(struct sighand_struct));
	if (sh != NULL)
    {
		set_sighand_count(sh, 0);
		int i;
		for (i = 0; i < sizeof(sh->action) / sizeof(sh->action[0]); ++i)
        {
			sh->action[i].sa_handler = SIG_DFL;
			sigset_initwith(sh->action[i].sa_mask, 0);
			sh->action[i].sa_flags = 0;
		}
		sem_init(&(sh->sig_sem), 1);
	}
	return sh;
}

// free a sighand_struct
void sighand_destroy(struct sighand_struct *sh)
{
	assert(sh != NULL && sighand_count(sh) == 0);
	kfree(sh);
}

// will the sign be ignored
static inline bool ignore_sig(int sign, struct proc_struct *proc)
{
	if (sign == SIGKILL || sign == SIGSTOP)
    {
		return 0;
	}
	struct sigaction *act = &(get_si(proc)->sighand->action[sign - 1]);
	return act->sa_handler == SIG_IGN || (act->sa_handler == SIG_DFL && (sign == SIGCONT || sign == SIGCHLD || sign == SIGWINCH || sign == SIGURG));
}

// do syscall sigaction
int do_sigaction(int sign, const struct sigaction *act, struct sigaction *old)
{
	assert(get_si(current)->sighand);
	cprintf("do_sigaction(): sign = %d, pid = %d\n", sign, current->pid);

    struct sigaction *k = &(get_si(current)->sighand->action[sign - 1]);
	if (k == NULL)
    {
		panic("kernel thread call sigaction (i guess)\n");
	}
    
	int ret = 0;
	struct mm_struct *mm = current->mm;
	lock_mm(mm);
	if (old != NULL && !copy_to_user(mm, old, k, sizeof(struct sigaction)))
    {
		unlock_mm(mm);
		ret = -E_INVAL;
		goto out;
	}
	if (act == NULL || !copy_from_user(mm, k, act, sizeof(struct sigaction), 1))
    {
		unlock_mm(mm);
		ret = -E_INVAL;
		goto out;
	}
	unlock_mm(mm);
	lock_sig(get_si(current)->sighand);
	sigset_del(k->sa_mask, SIGKILL);
	sigset_del(k->sa_mask, SIGSTOP);
	if (ignore_sig(sign, current))
    {
		// i'm not very sure that if we should lock
		remove_from_queue(sign, &(get_si(current)->signal->shared_pending));
		struct proc_struct *proc = current;
		do {
			remove_from_queue(sign, &(get_si(proc)->pending));
			sig_recalc_pending(proc);
			proc = next_thread(proc);
		} while (proc != current);
	}
	unlock_sig(get_si(current)->sighand);
out:
	return ret;
}

// do syscall sigpending
int do_sigpending(sigset_t *set)
{
	assert(get_si(current)->sighand);
	sigset_t pending;
	pending = get_si(current)->pending.signal | get_si(current)->signal->shared_pending.signal;
	pending &= get_si(current)->blocked;
	int ret = 0;
	if (set != NULL)
    {
		lock_mm(current->mm);
		if (!copy_to_user(current->mm, set, &pending, sizeof(sigset_t)))
        {
			ret = -E_INVAL;
		}
		unlock_mm(current->mm);
	}
	return ret;
}

// do syscall sigprocmask
int do_sigprocmask(int how, const sigset_t *set, sigset_t *old)
{
	assert(get_si(current)->signal);
	sigset_t new;
	int ret = -E_INVAL;
	if (set == NULL)
    {
		goto out;
	}
	struct mm_struct *mm = current->mm;
	lock_mm(mm);
	if (!copy_from_user(mm, &new, set, sizeof(sigset_t), 1))
    {
		unlock_mm(mm);
		goto out;
	}
	if (old != NULL && !copy_to_user(mm, old, &(get_si(current)->blocked), sizeof(sigset_t)))
    {
		unlock_mm(mm);
		goto out;
	}
	unlock_mm(mm);
	sigset_del(new, SIGKILL);
	sigset_del(new, SIGSTOP);
	ret = 0;
	switch (how)
    {
        case SIG_BLOCK:
            sigset_addmask(get_si(current)->blocked, new);
            break;
        case SIG_UNBLOCK:
            sigset_delmask(get_si(current)->blocked, new);
            break;
        case SIG_SETMASK:
            sigset_initwith(get_si(current)->blocked, new);
            break;
        default:
            ret = -E_INVAL;
	}
	sig_recalc_pending(current);
out:
	return ret;
}

// do syscall sigsuspend
int do_sigsuspend(sigset_t *pmask)
{
	struct mm_struct *mm = current->mm;
	sigset_t mask;
	lock_mm(mm);
	{
		if (!copy_from_user(mm, &mask, pmask, sizeof(sigset_t), 0))
        {
			unlock_mm(mm);
			return -E_INVAL;
		}
	}
	unlock_mm(mm);
	//kprintf("## %llx\n", mask);

	sigset_t set;
	sigset_initwith(set, 0);
	sigset_addmask(set, mask);
	sigset_del(set, SIGKILL);
	sigset_del(set, SIGSTOP);
	//kprintf("do_sigsuspend() %d\n", current->pid);
	//print_trapframe(current->tf);

	sigset_t old_blocked = get_si(current)->blocked;
	get_si(current)->blocked = set;

	while (1)
    {
		current->state = PROC_SLEEPING;
		current->wait_state = WT_SIGNAL;
		schedule();
		//kprintf("# HERE %08x %08x\n", current->state, current->wait_state);
		int ret;
		if ((ret = do_signal(current->tf, &old_blocked)) != 0)
        {
			return ret;
		}
	}
}

// add a signal to pending queue
int send_signal(int sign, struct siginfo_t *info, struct proc_struct *to, struct sigpending *pending)
{
	sigset_add(pending->signal, sign);
	return 0;
}

// wake up proc to handle its signal
void signal_wakeup(int sign, struct proc_struct *proc)
{
	proc->flags |= TIF_SIGPENDING;
	if (proc->state == PROC_SLEEPING
        && (proc->wait_state & WT_INTERRUPTED || sign == SIGKILL))
    {
		try_to_wakeup(proc);
	}
}

// send a signale to one proc
/* when calling this, lock_sig and local_intr_save is necessary */
int specific_send_sig_info(int sign, struct siginfo_t *info, struct proc_struct *to)
{
	bool intr_flag;
	int ret = 0;
	lock_sig(get_si(to)->sighand);
	local_intr_save(intr_flag);

	if (!sigismember(get_si(to)->blocked, sign) && ignore_sig(sign, to))
    {
		goto out;
	}
	if (sigismember(get_si(to)->pending.signal, sign))
    {
		goto out;;
	}
	if (send_signal(sign, info, to, &(get_si(to)->pending)) == 0
        && !sigismember(get_si(to)->blocked, sign))
    {
		signal_wakeup(sign, to);
	}
	ret = 1;
out:
	local_intr_restore(intr_flag);
	unlock_sig(get_si(to)->sighand);
	return ret;
}

// handle stop and continue signal
int handle_stop_signal(int sign, struct proc_struct *to)
{
	if (sign == SIGSTOP || sign == SIGTSTP
        || sign == SIGTTIN || sign == SIGTTOU)
    {
		remove_from_queue(SIGCONT, &(get_si(to)->signal->shared_pending));
		struct proc_struct *proc = current;
		do {
			remove_from_queue(SIGCONT, &(get_si(proc)->pending));
			sig_recalc_pending(proc);
			proc = next_thread(proc);
		} while (proc != current);
	}
    else if (sign == SIGCONT)
    {
		remove_from_queue(SIGSTOP, &(get_si(to)->signal->shared_pending));
		remove_from_queue(SIGTSTP, &(get_si(to)->signal->shared_pending));
		remove_from_queue(SIGTTIN, &(get_si(to)->signal->shared_pending));
		remove_from_queue(SIGTTOU, &(get_si(to)->signal->shared_pending));
		struct proc_struct *proc = current;
		do {
			remove_from_queue(SIGCONT, &(get_si(proc)->pending));
			remove_from_queue(SIGTSTP, &(get_si(proc)->pending));
			remove_from_queue(SIGTTIN, &(get_si(proc)->pending));
			remove_from_queue(SIGTTOU, &(get_si(proc)->pending));
			sig_recalc_pending(proc);
			proc = next_thread(proc);
		} while (proc != current);
	}
	return 0;
}

// does the proc want a signal
bool wants_signal(int sign, struct proc_struct *proc)
{
	if (sigismember(get_si(proc)->blocked, sign))
    {
		return 0;
	}
    else if (proc->flags & PF_EXITING || proc->state == PROC_ZOMBIE)
    {
		return 0;
	}
    else if (sign == SIGKILL)
    {
		return 1;
	}
    else
    {
		return !signal_pending(proc);
	}
}

// select a proc who wants signal from thread group
void group_complete_send(int sign, struct proc_struct *proc)
{
	struct proc_struct *pick;
	pick = get_si(proc)->signal->curr_target;
	if (pick == NULL)
		pick = get_si(proc)->signal->curr_target = proc;
    
	while (!wants_signal(sign, pick))
    {
		pick = next_thread(pick);
		if (pick == get_si(proc)->signal->curr_target)
        {
			return;
		}
	}
	get_si(proc)->signal->curr_target = pick;
	signal_wakeup(sign, pick);
}

// send a signal to thread group
int group_send_sig_info(int sign, struct siginfo_t *info, struct proc_struct *to)
{
	if (sign < 0 || sign > 64)
    {
		return -E_INVAL;
	}
	if (sign == 0 || get_si(to)->sighand == NULL)
    {
		return 0;
	}
	bool intr_flag;
	int ret = 0;
	lock_sig(get_si(to)->sighand);
	local_intr_save(intr_flag);
	handle_stop_signal(sign, to);
	if (!sigismember(get_si(to)->blocked, sign) && ignore_sig(sign, to))
    {
		goto out;
	}
	if (sigismember(get_si(to)->signal->shared_pending.signal, sign))
    {
		goto out;
	}
	if ((ret = send_signal(sign, info, to, &(get_si(to)->signal->shared_pending))) != 0)
    {
		goto out;
	}
	group_complete_send(sign, to);
	ret = 0;
out:
	local_intr_restore(intr_flag);
	unlock_sig(get_si(to)->sighand);
	return ret;
}

// prepare block for signal handler
int handle_signal(int sign, struct sigaction *act, sigset_t oldset, struct trapframe *tf)
{
	int ret = __sig_setup_frame(sign, act, oldset, tf);
	if (ret != 0)
    {
		return ret;
	}

	if ((act->sa_flags & SA_NODEFER) == 0)
    {
		lock_sig(get_si(current)->sighand);
		get_si(current)->blocked |= act->sa_mask;
		sigset_add(get_si(current)->blocked, sign);
		sig_recalc_pending(current);
		unlock_sig(get_si(current)->sighand);
	}
	return ret;
}

// stop thread group
void do_signal_stop(struct proc_struct *proc)
{
	struct proc_struct *next = proc;
	do {
		if (next->state == PROC_UNINIT
            || next->state == PROC_ZOMBIE
		    || next->flags & PF_EXITING)
        {
			continue;
		}
		if (next->state == PROC_SLEEPING
            && !(next->wait_state & WT_INTERRUPTED))
        {
			continue;
		}
		stop_proc(next, WT_SIGNAL);
	} while ((next = next_thread(next)) != proc);
    
	schedule();
}

// do the signals in current
int do_signal(struct trapframe *tf, sigset_t *old)
{
	assert(!trap_in_kernel(tf));
	if (!get_si(current)->signal || !get_si(current)->sighand)
		return 0;
    
	if (!signal_pending(current))
		return 0;
    
	int sign;
	if (old == NULL)
    {
		old = &get_si(current)->blocked;
	}

	while ((sign = dequeue_signal(current)) != 0)
    {
		cprintf("do_signal(): sign = %d, pid = %d\n", sign, current->pid);
		struct sigaction *act = &(get_si(current)->sighand->action[sign - 1]);
		if (sign == SIGKILL)
        {
			do_exit(-E_KILLED);
			break;
		}
        else if (sign == SIGSTOP)
        {
			do_signal_stop(current);
		}
        else if (act->sa_handler == SIG_IGN)
        {
			continue;
		}
        else if (act->sa_handler == SIG_DFL)
        {
			if (current == initproc)
            {
				continue;
			}
            else if (sign == SIGCONT || sign == SIGCHLD
                     || sign == SIGWINCH || sign == SIGURG)
            {
				continue;
			}
            else if (sign == SIGTSTP || sign == SIGTTIN || sign == SIGTTOU)
            {
				// because there's only one user, it's idle is always there, and no thread is orphane
				continue;
			}
            else
            {
				cprintf("do_signal() exit pid = %d\n", current->pid);
				do_exit(-E_KILLED);
				break;
			}
			/* user callback */
		}
        else
        {
			cprintf("do_signal() call user %d\n", sign);
			handle_signal(sign, act, *old, tf);
			if ((act->sa_flags & SA_ONESHOT) != 0)
            {
				act->sa_handler = SIG_DFL;
			}
			return sign;
		}
	}
	return sign;
}

int raise_signal(struct proc_struct *proc, int sign, bool group)
{
	struct siginfo_t info;
	info.si_signo = sign;
	info.si_errno = 0;
	info.si_code = SI_USER;
	if (group)
    {
		return group_send_sig_info(sign, &info, proc);
	}
    else
    {
		return specific_send_sig_info(sign, &info, proc);
	}
}

int do_sigtkill(int pid, int sign)
{
	struct proc_struct *proc = find_proc(pid);
	if (proc == NULL || proc->state == PROC_ZOMBIE)
    {
		return -E_INVAL;
	}
	return raise_signal(proc, sign, 0);
}

int do_sigkill(int pid, int sign)
{
	struct proc_struct *proc = find_proc(pid);
	if (proc == NULL || proc->state == PROC_ZOMBIE)
    {
		return -E_INVAL;
	}
	cprintf("do_sigkill: pid=%d sig=%d\n", pid, sign);
	return raise_signal(proc, sign, 1);
}

// do syscall sigreturn, reset the user stack and eip
int do_sigreturn()
{
    struct mm_struct *mm = current->mm;
    if (!current)
        return -E_INVAL;
    
    struct sighand_struct *sighand = current->signal_info.sighand;
    if (!sighand)
        return -E_INVAL;
    
    struct sigframe *kframe = kmalloc(sizeof(struct sigframe));
    if (!kframe)
        return -E_NO_MEM;
    
    struct sigframe *frame = (struct sigframe *)(current->tf->tf_esp);
    lock_mm(mm);
    {
        if (!copy_from_user(mm, kframe, frame, sizeof(struct sigframe), 0))
        {
            unlock_mm(mm);
            kfree(kframe);
            return -E_INVAL;
        }
    }
    unlock_mm(mm);
    /* check the trapframe */
    if (trap_in_kernel(&kframe->tf))
    {
        do_exit(-E_KILLED);
        return -E_INVAL;
    }
    
    lock_sig(sighand);
    current->signal_info.blocked = kframe->old_blocked;
    sig_recalc_pending(current);
    unlock_sig(sighand);
    
    *(current->tf) = kframe->tf;
    kfree(kframe);
    
    return 0;
}

// do syscall sigaltstack
int do_sigaltstack(const stack_t *ss, stack_t *old)
{
	int ret = -E_INVAL;
	if (ss == NULL)
    {
		return ret;
	}

	struct mm_struct *mm = current->mm;
	stack_t stack = { get_si(current)->sas_ss_sp, 0, get_si(current)->sas_ss_size };
	lock_mm(mm);
	if (old != NULL && !copy_to_user(mm, old, &stack, sizeof(stack_t)))
    {
		goto out;
	}
	if (copy_from_user(mm, &stack, ss, sizeof(stack_t), 1))
    {
		get_si(current)->sas_ss_sp = stack.sp;
		get_si(current)->sas_ss_size = stack.size;
	}
	unlock_mm(mm);
out:
	return ret;
}

int do_sigwaitinfo(const sigset_t *setp, struct siginfo_t *info)
{
	sigset_t set;
	struct mm_struct *mm = current->mm;
	assert(mm != NULL);
	lock_mm(mm);
	if (setp == NULL || !copy_from_user(mm, &set, setp, sizeof(sigset_t), 0))
    {
		assert(0);
		unlock_mm(mm);
		return -1;
	}
	unlock_mm(mm);

	sigset_add(set, SIGKILL);
	sigset_add(set, SIGSTOP);
	sigset_t old_blocked = get_si(current)->blocked;
	get_si(current)->blocked = ~set;
	cprintf("do_sigwaitinfo(): set = %016llx, pid = %d\n", set, current->pid);

	while (1)
    {
		current->state = PROC_SLEEPING;
		current->wait_state = WT_SIGNAL;
		schedule();
		int sign = do_signal(current->tf, &old_blocked);
		if (sign != 0)
        {
			cprintf("do_sigwaitinfo(): set = %016llx, sign = %d, pid = %d\n", set, sign, current->pid);
			return sign;
		}
	}
}
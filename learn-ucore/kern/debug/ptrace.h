#ifndef _LINUX_PTRACE_H
#define _LINUX_PTRACE_H
/* ptrace.h */
/* structs and defines to help the user use the ptrace system call. */

/* has the defines to get at the registers. */

#define PTRACE_TRACEME		    0
#define PTRACE_PEEKTEXT		    1
#define PTRACE_PEEKDATA		    2
#define PTRACE_PEEKUSR		    3
#define PTRACE_POKETEXT		    4
#define PTRACE_POKEDATA		    5
#define PTRACE_POKEUSR		    6
#define PTRACE_CONT		        7
#define PTRACE_KILL		        8
#define PTRACE_SINGLESTEP	    9

#define PTRACE_ATTACH		    0x10
#define PTRACE_DETACH		    0x11

#define PTRACE_SYSCALL		    24

/* use ptrace (3 or 6, pid, PT_EXCL, data); to read or write
   the processes registers. */

#define EBX         0
#define ECX         1
#define EDX         2
#define ESI         3
#define EDI         4
#define EBP         5
#define EAX         6
#define DS          7
#define ES          8
#define FS          9
#define GS          10
#define ORIG_EAX    11
#define EIP         12
#define CS          13
#define EFL         14
#define UESP        15
#define SS          16

/* this struct defines the way the registers are stored on the
   stack during a system call. */

struct pt_regs
{
    long ebx;
    long ecx;
    long edx;
    long esi;
    long edi;
    long ebp;
    long eax;
    unsigned short ds, __dsu;
    unsigned short es, __esu;
    unsigned short fs, __fsu;
    unsigned short gs, __gsu;
    long orig_eax;
    long eip;
    unsigned short cs, __csu;
    long eflags;
    long esp;
    unsigned short ss, __ssu;
};

struct user_i387_struct
{
    long cwd;
    long swd;
    long twd;
    long fip;
    long fcs;
    long foo;
    long fos;
    long st_space[20];    /* 8*10 bytes for each FP-reg = 80 bytes */
};

/* When the kernel dumps core, it starts by dumping the user struct -
 this will be used by gdb to figure out where the data and stack segments
 are within the file, and what virtual addresses to use. */
struct user
{
    /* We start with the registers, to mimic the way that "memory" is returned
     from the ptrace(3,...) function.  */
    struct pt_regs regs;        /* Where the registers are actually stored */
    /* ptrace does not yet supply these.  Someday.... */
    int u_fpvalid;        /* True if math co-processor being used. */
    /* for this mess. Not yet used. */
    struct user_i387_struct i387;    /* Math Co-processor registers. */
    /* The rest of this junk is to help gdb figure out what goes where */
    unsigned long int u_tsize;    /* Text segment size (pages). */
    unsigned long int u_dsize;    /* Data segment size (pages). */
    unsigned long int u_ssize;    /* Stack segment size (pages). */
    unsigned long start_code;     /* Starting virtual address of text. */
    unsigned long start_stack;    /* Starting virtual address of stack area.
                                   This is actually the bottom of the stack,
                                   the top of the stack is always found in the
                                   esp register.  */
    long int signal;             /* Signal that caused the core dump. */
    int reserved;            /* No longer used */
    struct pt_regs *u_ar0;    /* Used by gdb to help find the values for */
    /* the registers. */
    struct user_i387_struct *u_fpstate;    /* Math Co-processor pointer. */
    unsigned long magic;        /* To uniquely identify a core file */
    char u_comm[32];        /* User command that was responsible */
    int u_debugreg[8];
};

#define NBPG 4096
#define UPAGES 1
#define HOST_TEXT_START_ADDR (u.start_code)
#define HOST_STACK_END_ADDR (u.start_stack + u.u_ssize * NBPG)

int do_ptrace(long request, long pid, long addr, long data);

#endif

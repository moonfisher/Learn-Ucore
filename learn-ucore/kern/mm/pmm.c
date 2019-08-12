#include "defs.h"
#include "x86.h"
#include "stdio.h"
#include "string.h"
#include "mmu.h"
#include "memlayout.h"
#include "pmm.h"
#include "default_pmm.h"
#include "sync.h"
#include "error.h"
#include "swap.h"
#include "vmm.h"
#include "slab.h"
#include "unistd.h"
#include "trap.h"
#include "slab.h"
#include "cpu.h"

/* *
 * Task State Segment:
 *
 * The TSS may reside anywhere in memory. A special segment register called
 * the Task Register (TR) holds a segment selector that points a valid TSS
 * segment descriptor which resides in the GDT. Therefore, to use a TSS
 * the following must be done in function gdt_init:
 *   - create a TSS descriptor entry in GDT
 *   - add enough information to the TSS in memory as needed
 *   - load the TR register with a segment selector for that segment
 *
 * There are several fileds in TSS for specifying the new stack pointer when a
 * privilege level change happens. But only the fields SS0 and ESP0 are useful
 * in our os kernel.
 *
 * The field SS0 contains the stack segment selector for CPL = 0, and the ESP0
 * contains the new ESP value for CPL = 0. When an interrupt happens in protected
 * mode, the x86 CPU will look in the TSS for SS0 and ESP0 and load their value
 * into SS and ESP respectively.
 * */
/* 0xC0158FC0
{
    ts_link = 0x0,
    ts_esp0 = 0xC0155000,
    ts_ss0 = 0x10,
    ts_padding1 = 0x0,
    ts_esp1 = 0x0,
    ts_ss1 = 0x0,
    ts_padding2 = 0x0,
    ts_esp2 = 0x0,
    ts_ss2 = 0x0,
    ts_padding3 = 0x0,
    ts_cr3 = 0x0,
    ts_eip = 0x0,
    ts_eflags = 0x0,
    ts_eax = 0x0,
    ts_ecx = 0x0,
    ts_edx = 0x0,
    ts_ebx = 0x0,
    ts_esp = 0x0,
    ts_ebp = 0x0,
    ts_esi = 0x0,
    ts_edi = 0x0,
    ts_es = 0x0,
    ts_padding4 = 0x0,
    ts_cs = 0x0,
    ts_padding5 = 0x0,
    ts_ss = 0x0,
    ts_padding6 = 0x0,
    ts_ds = 0x0,
    ts_padding7 = 0x0,
    ts_fs = 0x0,
    ts_padding8 = 0x0,
    ts_gs = 0x0,
    ts_padding9 = 0x0,
    ts_ldt = 0x0,
    ts_padding10 = 0x0,
    ts_t = 0x0,
    ts_iomb = 0x0
}
*/
//static struct taskstate ts = {0};

// 任务门描述符
struct taskstate taskgate = {0};

// 调用门描述符
static struct gatedesc callgate = {0};

// virtual address of physicall page array 0xC015D000
struct Page *pages;
// amount of physical memory (in pages) 0x7FE0
size_t npage = 0;

// virtual address of boot-time page directory 0xC0156000
// 内核页目录，不用用户进程有属于自己的页目录
#if ASM_NO_64
    extern pde_t __boot_pgdir;
#else
    pde_t __boot_pgdir;
#endif
pde_t *boot_pgdir = &__boot_pgdir;
// physical address of boot-time page directory 0x156000
uintptr_t boot_cr3;

// physical memory management
const struct pmm_manager *pmm_manager;

/* *
 * The page directory entry corresponding to the virtual address range
 * [VPT, VPT + PTSIZE) points to the page directory itself. Thus, the page
 * directory is treated as a page table as well as a page directory.
 *
 * One result of treating the page directory as a page table is that all PTEs
 * can be accessed though a "virtual page table" at virtual address VPT. And the
 * PTE for number n is stored in vpt[n].
 *
 * A second consequence is that the contents of the current page directory will
 * always available at virtual address PGADDR(PDX(VPT), PDX(VPT), 0), to which
 * vpd is set bellow.
 * */
/*
 vpd 和 vpt 的作用
 在物理地址空间里，页目录，页表并不是地址连续的，一开始并不会根据虚拟地址空间去构造所有页表，
 这样对于内存空间是很大的浪费，只有用到的时候才会临时申请内存去创建（比如发生缺页中断需要分配物理页面）。
 这样对于页目录和页表的遍历访问就比较麻烦，如果需要按虚拟地址的地址顺序显示整个页目录表和页表的内容，
 则要查找页目录表的页目录表项内容，根据页目录表项内容找到页表的物理地址，再转换成对应的虚地址，然后访
 问页表的虚地址，搜索整个页表的每个页目录项来实现。
 
 ucore 做了一个很巧妙的地址自映射设计，参考 memlayout.h 虚拟地址分配图，
 把页目录表和页表放在一个连续的 4M (4M 可以映射 4G 空间，但实际内存映射只映射 896M，用不了 4M 这么多)
 虚拟地址空间中，并设置页目录表自身的虚地址 <--> 物理地址映射关系。
 这样在已知页目录表起始虚地址的情况下，通过连续扫描这特定的 4M 虚拟地址空间，就很容易访问每个页目录表
 项和页表项内容。
 
 这个也是用户态访问内核态页表项的非常精巧的方式，对于一个用户态的虚拟地址 va (4k 对齐才行) 来说，
 vpd[va >> 22] 读出的是 page table，而 vpt[va >> 12] 读出的是页表项 pte，
 vpt[va >> 12] 仅当 vpd[va >> 22] 存在时才有意义。
*/
// 0xFAC00000 = 1111101011 0000000000 000000000000 = 0x3EB 0x0 0x0
pte_t * const vpt = (pte_t *)VPT;   // 0xFAC00000
// 0xFAFEB000 = 1111101011 1111101011 000000000000 = 0x3EB 0x3EB 0x0
// (gdb) x /32hx boot_pgdir + 0x300 内容等于 (gdb) x /32hx vpd + 0x300
pde_t * const vpd = (pde_t *)PGADDR(PDX(VPT), PDX(VPT), 0); // 0xFAFEB000

/* *
 * Global Descriptor Table:
 *
 * The kernel and user segments are identical (except for the DPL). To load
 * the %ss register, the CPL must equal the DPL. Thus, we must duplicate the
 * segments for the user and the kernel. Defined as follows:
 *   - 0x0 :  unused (always faults -- for trapping NULL far pointers)
 *   - 0x8 :  kernel code segment
 *   - 0x10:  kernel data segment
 *   - 0x18:  user code segment
 *   - 0x20:  user data segment
 *   - 0x28:  defined for tss, initialized in gdt_init
 * */
/* 0xC0155A20
{
    {
        sd_lim_15_0 = 0x0,
        sd_base_15_0 = 0x0,
        sd_base_23_16 = 0x0,
        sd_type = 0x0,
        sd_s = 0x0,
        sd_dpl = 0x0,
        sd_p = 0x0,
        sd_lim_19_16 = 0x0,
        sd_avl = 0x0,
        sd_rsv1 = 0x0,
        sd_db = 0x0,
        sd_g = 0x0,
        sd_base_31_24 = 0x0
    },
 
    {
        sd_lim_15_0 = 0xffff,
        sd_base_15_0 = 0x0,
        sd_base_23_16 = 0x0,
        sd_type = 0xa,  // 1010
        sd_s = 0x1,
        sd_dpl = 0x0,
        sd_p = 0x1,
        sd_lim_19_16 = 0xf,
        sd_avl = 0x0,
        sd_rsv1 = 0x0,
        sd_db = 0x1,
        sd_g = 0x1,
        sd_base_31_24 = 0x0
    },
 
    {
        sd_lim_15_0 = 0xffff,
        sd_base_15_0 = 0x0,
        sd_base_23_16 = 0x0,
        sd_type = 0x3,  // 0011
        sd_s = 0x1,
        sd_dpl = 0x0,
        sd_p = 0x1,
        sd_lim_19_16 = 0xf,
        sd_avl = 0x0,
        sd_rsv1 = 0x0, 
        sd_db = 0x1,
        sd_g = 0x1,
        sd_base_31_24 = 0x0
    },
 
    {
        sd_lim_15_0 = 0xffff,
        sd_base_15_0 = 0x0,
        sd_base_23_16 = 0x0,
        sd_type = 0xa,  // 1010
        sd_s = 0x1,
        sd_dpl = 0x3,
        sd_p = 0x1,
        sd_lim_19_16 = 0xf,
        sd_avl = 0x0,
        sd_rsv1 = 0x0,
        sd_db = 0x1,
        sd_g = 0x1,
        sd_base_31_24 = 0x0
    },
 
    {
        sd_lim_15_0 = 0xffff,
        sd_base_15_0 = 0x0,
        sd_base_23_16 = 0x0,
        sd_type = 0x3,  // 0011
        sd_s = 0x1,
        sd_dpl = 0x3,
        sd_p = 0x1,
        sd_lim_19_16 = 0xf,
        sd_avl = 0x0,
        sd_rsv1 = 0x0,
        sd_db = 0x1,
        sd_g = 0x1,
        sd_base_31_24 = 0x0
    },
 
    {
        sd_lim_15_0 = 0x68,
        sd_base_15_0 = 0x8fc0,
        sd_base_23_16 = 0x15,
        sd_type = 0xb,  // 1011
        sd_s = 0x0,
        sd_dpl = 0x0,
        sd_p = 0x1,
        sd_lim_19_16 = 0x0,
        sd_avl = 0x0,
        sd_rsv1 = 0x0,
        sd_db = 0x1,
        sd_g = 0x0,
        sd_base_31_24 = 0xC0
     }
}
 */
struct segdesc gdt[NSEGS] = {
    SEG_NULL,
    [SEG_KTEXT]     = SEG_NULL,
    [SEG_KDATA]     = SEG_NULL,
    [SEG_UTEXT]     = SEG_NULL,
    [SEG_UDATA]     = SEG_NULL,
    [SEG_CALL_GATE] = SEG_NULL,
    [SEG_TASK_GATE] = SEG_NULL,
    [SEG_KCPU]      = SEG_NULL,
    [SEG_TSS]       = SEG_NULL,
};

/*
    {
        pd_lim = 0x2f,
        pd_base = 0xC0155A20
    }
*/
#if ASM_NO_64
    struct pseudodesc gdt_pd = {sizeof(gdt) - 1, (uintptr_t)gdt};
#else
    struct pseudodesc gdt_pd;
#endif

static void check_alloc_page(void);
static void check_pgdir(void);
static void check_boot_pgdir(void);

/* *
 * lgdt - load the global descriptor table register and reset the
 * data/code segement registers for kernel.
 * */
void lgdt(struct pseudodesc *pd)
{
#if ASM_NO_64
    asm volatile ("lgdt (%0)" :: "r" (pd));
    asm volatile ("movw %%ax, %%gs" :: "a" (USER_DS));
    asm volatile ("movw %%ax, %%fs" :: "a" (USER_DS));
    asm volatile ("movw %%ax, %%es" :: "a" (KERNEL_DS));
    asm volatile ("movw %%ax, %%ds" :: "a" (KERNEL_DS));
    asm volatile ("movw %%ax, %%ss" :: "a" (KERNEL_DS));
    // reload cs 长跳转才能修改 cs 寄存器，通过跳转指令重新更新 cs 到内核代码段
    asm volatile ("ljmp %0, $1f\n 1:\n" :: "i" (KERNEL_CS));
#endif
}

/* *
 * load_esp0 - change the ESP0 in default task state segment,
 * so that we can use different kernel stack when we trap frame
 * user to kernel.
 
 用户栈和内核栈的区别
 内核栈：内存中属于操作系统空间的一块区域，空间不大，目前设置的 8k，
 每个 proc task 都有属于自己的内核栈，task 不同，内核栈就不同
 
 作用：
  1 保存中断现场，对于嵌套中断，被中断程序的现场信息一次压入系统栈，中断返回时逆序弹出
  2 保存操作系统程序相互调用的参数，返回值，以及函数的局部变量
 
 用户栈：是用户进程空间的一块区域，用于保存用户空间子程序间调用的参数，返回值以及局部变量。
 
 为什么不能只用一个栈？
 
 1，如果只用系统栈，系统栈一般大小有限，用户程序调用次数可能很多。如果中断有16个优先级，
    那么系统栈一般大小为15（只需保存15个低优先级中断，另一个高优先级中断在运行）用户程序调用次数很多，
    那样15次子程序调用以后的子程序的参数，返回值，局部变量就不能保存，用户程序也就不能正常运行。
 2，如果只用用户栈，系统程序需要在某种保护下运行，而用户栈在用户空间不能提供相应的保护措施。
 */
void load_esp0(uintptr_t esp0)
{
    thiscpu->cpu_ts.ts_esp0 = esp0;
}

/* gdt_init - initialize the default GDT and TSS */
/*
 tss的作用举例：
 保存不同特权级别下任务所使用的寄存器，特别重要的是esp，因为比如中断后，涉及特权级切换时(一个任务切换)，
 首先要切换栈，这个栈显然是内核栈，那么如何找到该栈的地址呢，这需要从tss段中得到，
 这样后续的执行才有所依托(在x86机器上，c语言的函数调用是通过栈实现的)。
 只要涉及地特权环到高特权环的任务切换，都需要找到高特权环对应的栈，因此需要esp2，esp1，esp0起码三个esp，
 然而Linux只使用esp0。
 
 tss是什么：
 tss是一个段，段是x86的概念，在保护模式下，段选择符参与寻址，段选择符在段寄存器中，而tss段则在tr寄存器中。
 
 intel的建议：为每一个进程准备一个独立的tss段，进程切换的时候切换tr寄存器使之指向该进程对应的tss段，
 然后在任务切换时(比如涉及特权级切换的中断)使用该段保留所有的寄存器。
 
 Linux的实际做法：
 
 1.Linux没有为每一个进程都准备一个tss段，而是每一个cpu使用一个tss段，tr寄存器保存该段。
 进程切换时，只更新唯一tss段中的esp0字段到新进程的内核栈。
 
 2.Linux的tss段中只使用esp0和iomap等字段，不用它来保存寄存器，在一个用户进程被中断进入ring0的时候，
 tss中取出esp0，然后切到esp0，其它的寄存器则保存在esp0指示的内核栈上而不保存在tss中。
 
 3.结果，Linux中每一个cpu只有一个tss段，tr寄存器永远指向它。符合x86处理器的使用规范，
 但不遵循intel的建议，这样的后果是开销更小了，因为不必切换tr寄存器了。
 
 4.在Linux中，对于同一个CPU，所有的进程都使用一个TSS，只是在进程切换时，
 被切换到的进程将会把自己的ESP0保存到TSS.ESP0中去，那为什么不把自己的SS0也保存到TSS.SS0中 呢，
 这是因为所有进程的SS0都是统一的，为内核的SS，而内核在初始化的时候，已经将该TSS.SS0设置为自己的SS，
 因此无需继续设置SS0。
 
 发生中断后：
 
 CPU 会根据 CPL 和中断服务例程的段描述符的 DPL 信息确认是否发生了特权级的转换。
 比如当前程序正运行在用户态，而中断程序是运行在内核态的，则意味着发生了特权级的转换，
 这时 CPU 会从当前进程的 TSS 信息（该信息在内存中的起始地址存在 TR 寄存器中）里取得该进程的内核栈地址，
 即包括内核态的 ss 和 esp 的值，并立即将系统当前使用的栈切换成该进程的内核栈。
 这个栈就是即将运行的中断服务程序要使用的栈。
 紧接着就将当前进程正在使用的用户态的 ss 和 esp 压到进程的内核栈中保存起来，中断处理结束后还要返回到用户栈
*/
static void gdt_init(void)
{
    // 第一个段描述符必须为全 0
    gdt[0].sd_lim_15_0 = 0;
    gdt[0].sd_base_15_0 = 0;
    gdt[0].sd_base_23_16 = 0;
    gdt[0].sd_type = 0;
    gdt[0].sd_s = 0;
    gdt[0].sd_dpl = 0;
    gdt[0].sd_p = 0;
    gdt[0].sd_lim_19_16 = 0;
    gdt[0].sd_avl = 0;
    gdt[0].sd_rsv1 = 0;
    gdt[0].sd_db = 0;
    gdt[0].sd_g = 0;
    gdt[0].sd_base_31_24 = 0;
 
    // 内核代码段描述符
    gdt[SEG_KTEXT].sd_lim_15_0 = 0xFFFF;
    gdt[SEG_KTEXT].sd_base_15_0 = 0;
    gdt[SEG_KTEXT].sd_base_23_16 = 0;
    gdt[SEG_KTEXT].sd_type = STA_X | STA_R;
    gdt[SEG_KTEXT].sd_s = 1;    // 内核也是应用段，这里为 1
    gdt[SEG_KTEXT].sd_dpl = DPL_KERNEL;
    gdt[SEG_KTEXT].sd_p = 1;
    gdt[SEG_KTEXT].sd_lim_19_16 = 0xF;
    gdt[SEG_KTEXT].sd_avl = 0;
    gdt[SEG_KTEXT].sd_rsv1 = 0;
    gdt[SEG_KTEXT].sd_db = 1;
    gdt[SEG_KTEXT].sd_g = 1;
    gdt[SEG_KTEXT].sd_base_31_24 = 0;
    
    // 内核数据段描述符
    gdt[SEG_KDATA].sd_lim_15_0 = 0xFFFF;
    gdt[SEG_KDATA].sd_base_15_0 = 0;
    gdt[SEG_KDATA].sd_base_23_16 = 0;
    gdt[SEG_KDATA].sd_type = STA_W;
    gdt[SEG_KDATA].sd_s = 1;    // 内核也是应用段，这里为 1
    gdt[SEG_KDATA].sd_dpl = DPL_KERNEL;
    gdt[SEG_KDATA].sd_p = 1;
    gdt[SEG_KDATA].sd_lim_19_16 = 0xF;
    gdt[SEG_KDATA].sd_avl = 0;
    gdt[SEG_KDATA].sd_rsv1 = 0;
    gdt[SEG_KDATA].sd_db = 1;
    gdt[SEG_KDATA].sd_g = 1;
    gdt[SEG_KDATA].sd_base_31_24 = 0;
    
    // 用户代码段描述符
    gdt[SEG_UTEXT].sd_lim_15_0 = 0xFFFF;
    gdt[SEG_UTEXT].sd_base_15_0 = 0;
    gdt[SEG_UTEXT].sd_base_23_16 = 0;
    gdt[SEG_UTEXT].sd_type = STA_X | STA_R;
    gdt[SEG_UTEXT].sd_s = 1;    // 用户段也是应用段，这里为 1
    gdt[SEG_UTEXT].sd_dpl = DPL_USER;
    gdt[SEG_UTEXT].sd_p = 1;
    gdt[SEG_UTEXT].sd_lim_19_16 = 0xF;
    gdt[SEG_UTEXT].sd_avl = 0;
    gdt[SEG_UTEXT].sd_rsv1 = 0;
    gdt[SEG_UTEXT].sd_db = 1;
    gdt[SEG_UTEXT].sd_g = 1;
    gdt[SEG_UTEXT].sd_base_31_24 = 0;
    
    // 用户数据段描述符
    gdt[SEG_UDATA].sd_lim_15_0 = 0xFFFF;
    gdt[SEG_UDATA].sd_base_15_0 = 0;
    gdt[SEG_UDATA].sd_base_23_16 = 0;
    gdt[SEG_UDATA].sd_type = STA_W;
    gdt[SEG_UDATA].sd_s = 1;    // 用户段也是应用段，这里为 1
    gdt[SEG_UDATA].sd_dpl = DPL_USER;
    gdt[SEG_UDATA].sd_p = 1;
    gdt[SEG_UDATA].sd_lim_19_16 = 0xF;
    gdt[SEG_UDATA].sd_avl = 0;
    gdt[SEG_UDATA].sd_rsv1 = 0;
    gdt[SEG_UDATA].sd_db = 1;
    gdt[SEG_UDATA].sd_g = 1;
    gdt[SEG_UDATA].sd_base_31_24 = 0;

    // tss 任务描述符
    gdt[SEG_TSS].sd_lim_15_0 = (sizeof(taskgate)) & 0xffff;
    gdt[SEG_TSS].sd_base_15_0 = ((uintptr_t)&taskgate) & 0xffff;
    gdt[SEG_TSS].sd_base_23_16 = (((uintptr_t)&taskgate) >> 16) & 0xff;
    gdt[SEG_TSS].sd_type = STS_T32A;    // 说明这个段是 tss 段
    gdt[SEG_TSS].sd_s = 0;              // tss 是系统段，这里必须为 0
    gdt[SEG_TSS].sd_dpl = DPL_KERNEL;
    gdt[SEG_TSS].sd_p = 1;
    gdt[SEG_TSS].sd_lim_19_16 = (unsigned)(sizeof(taskgate)) >> 16;
    gdt[SEG_TSS].sd_avl = 0;
    gdt[SEG_TSS].sd_rsv1 = 0;
    gdt[SEG_TSS].sd_db = 1;
    gdt[SEG_TSS].sd_g = 0;      // tss 段界限粒度是字节，不是 4k
    gdt[SEG_TSS].sd_base_31_24 = (unsigned)((uintptr_t)&taskgate) >> 24;

    /*
     调用门虽然是 CPU 提供给使用者提权的一种手段，通过调用门也可以实现各种系统调用，但是
     linux 中却并未使用。
     在 linux 中，大量使用了中断门来进行提权，包括后面的系统调用，都是采用中断的方式实现。
     另外，所谓的后门，其实有很多，比如中断门，陷阱门，任务门。它们都可以实现提权。
     和中断门稍稍有点不一样的地方是，调用门提权会在堆栈中少压入一个值 EFLAGS.
     调用门描述符可以在 GDT 或 LDT 中，但是不能在中断描述符(IDT)中
     */
#if ASM_NO_64
    extern uintptr_t __vectors[];
#else
    uintptr_t __vectors[1];
#endif
    // 注意这里是一个门结构，不是段结构，和段结构一样，都是 8 字节，64位
    callgate.gd_off_15_0 = (uint32_t)(__vectors[T_CALLGATE]) & 0xffff;
    callgate.gd_ss = GD_KTEXT;      // 段选择子指向内核段，调用之后特权级提升，堆栈切换
    // 这里配置传递参数个数为 1，这样内核栈中的结构也可以用 struct trapframe 中断帧来模拟
    callgate.gd_args = 1;
    callgate.gd_rsv1 = 0;
    callgate.gd_type = STS_CG32;    // 这是调用门
    callgate.gd_s = 0;              // 调用门是系统段，这里必须为 0
    callgate.gd_dpl = DPL_USER;     // 这里要设置为 DPL_USER
    callgate.gd_p = 1;
    callgate.gd_off_31_16 = (uint32_t)(__vectors[T_CALLGATE]) >> 16;
    memcpy(&gdt[SEG_CALL_GATE], &callgate, sizeof(callgate));

    // tss 任务门描述符
    gdt[SEG_TASK_GATE].sd_lim_15_0 = (sizeof(taskgate)) & 0xffff;
    gdt[SEG_TASK_GATE].sd_base_15_0 = ((uintptr_t)&taskgate) & 0xffff;
    gdt[SEG_TASK_GATE].sd_base_23_16 = (((uintptr_t)&taskgate) >> 16) & 0xff;
    gdt[SEG_TASK_GATE].sd_type = STS_TG;    // 说明这个段是 tss 段
    gdt[SEG_TASK_GATE].sd_s = 0;              // tss 是系统段，这里必须为 0
    gdt[SEG_TASK_GATE].sd_dpl = DPL_KERNEL;
    gdt[SEG_TASK_GATE].sd_p = 1;
    gdt[SEG_TASK_GATE].sd_lim_19_16 = (unsigned)(sizeof(taskgate)) >> 16;
    gdt[SEG_TASK_GATE].sd_avl = 0;
    gdt[SEG_TASK_GATE].sd_rsv1 = 0;
    gdt[SEG_TASK_GATE].sd_db = 1;
    gdt[SEG_TASK_GATE].sd_g = 0;  // tss 段界限粒度是字节，不是 4k
    gdt[SEG_TASK_GATE].sd_base_31_24 = (unsigned)((uintptr_t)&taskgate) >> 24;

    // reload all segment registers
    lgdt(&gdt_pd);
}

//init_pmm_manager - initialize a pmm_manager instance
// 内存管理代码模块化，组件化，后续可以直接实现别的 default_pmm_manager
static void init_pmm_manager(void)
{
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

//init_memmap - call pmm->init_memmap to build Page struct for free memory
// 空闲内存按照 4k 一页用双向链表连起来
static void init_memmap(struct Page *base, size_t n)
{
    pmm_manager->init_memmap(base, n);
}

//alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE memory 
struct Page *alloc_pages(size_t n)
{
    struct Page *page = NULL;
    bool intr_flag;

    while (1)
    {
        local_intr_save(intr_flag);
        {
            page = pmm_manager->alloc_pages(n);
        }
        local_intr_restore(intr_flag);

        if (page != NULL || n > 1 || swap_init_ok == 0)
            break;

        struct mm_struct *mm = current->mm;
        extern struct mm_struct *check_mm_struct;
        if (check_mm_struct)
        {
            mm = check_mm_struct;
        }

        //cprintf("page %x, call swap_out in alloc_pages %d\n",page, n);
        swap_out(mm, n, 0);
    }
    //cprintf("n %d,get page %x, No %d in alloc_pages\n",n,page,(page-pages));
    return page;
}

//free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory 
void free_pages(struct Page *base, size_t n)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

//nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE) 
//of current free memory
size_t nr_free_pages(void)
{
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

/* pmm_init - initialize the physical memory management */
/*
 DRAM:          0x00000 - 0x9FFFF, 640K;
 Peripherals:   0xA0000 - 0xEFFFF, 320K (Graphic memory mapping: 0xB8000 - 0xBFFFF).
 BIOS ROM:      0xF0000 - 0xFFFFF, 64K;
 
 e820map: qemu 模拟 512M 内存后的分布
     memory: 0009fc00, [00000000, 0009fbff], type = 1.
     memory: 00000400, [0009fc00, 0009ffff], type = 2.
     memory: 00010000, [000f0000, 000fffff], type = 2.
     memory: 1fee0000, [00100000, 1ffdffff], type = 1.
     memory: 00020000, [1ffe0000, 1fffffff], type = 2.
     memory: 00040000, [fffc0000, ffffffff], type = 2.
 
 e820map: qemu 模拟 1024M 内存后的分布
     memory: 0009fc00, [00000000, 0009fbff], type = 1.
     memory: 00000400, [0009fc00, 0009ffff], type = 2.
     memory: 00010000, [000f0000, 000fffff], type = 2.
     memory: 3fee0000, [00100000, 3ffdffff], type = 1.
     memory: 00020000, [3ffe0000, 3fffffff], type = 2.
     memory: 00040000, [fffc0000, ffffffff], type = 2.
 
 e820map: qemu 模拟 4096M 内存后的分布
     memory: 0009fc00, [00000000, 0009fbff],    type = 1.
     memory: 00000400, [0009fc00, 0009ffff],    type = 2.
     memory: 00010000, [000f0000, 000fffff],    type = 2.
     memory: bfee0000, [00100000, bffdffff],    type = 1.
     memory: 00020000, [bffe0000, bfffffff],    type = 2.
     memory: 00040000, [fffc0000, ffffffff],    type = 2.
     memory: 40000000, [100000000, 13fffffff],  type = 1.
 
 e820map: qemu 模拟 8192M 内存后的分布
     memory: 0009fc00, [00000000, 0009fbff],    type = 1.
     memory: 00000400, [0009fc00, 0009ffff],    type = 2.
     memory: 00010000, [000f0000, 000fffff],    type = 2.
     memory: bfee0000, [00100000, bffdffff],    type = 1.
     memory: 00020000, [bffe0000, bfffffff],    type = 2.
     memory: 00040000, [fffc0000, ffffffff],    type = 2.
     memory: 140000000, [100000000, 23fffffff], type = 1.
*/
static void page_init(void)
{
    //可用内存的数据结构在启动时通过 probe_memory 函数探测得来，结果存放在 0x8000 处
    //这里加上 KERNBASE 是因为此时进入了分页模式
    struct e820map *memmap = (struct e820map *)(0x8000 + KERNBASE);
    uint64_t maxpa = 0;

    cprintf("e820map:\n");
    int i = 0;
    for (i = 0; i < memmap->nr_map; i++)
    {
        uint64_t begin = memmap->map[i].addr, end = begin + memmap->map[i].size;
        cprintf("  memory: %08llx, [%08llx, %08llx], type = %d.\n", memmap->map[i].size, begin, end - 1, memmap->map[i].type);
        if (memmap->map[i].type == E820_ARM)
        {
            if (maxpa < end && begin < KMEMSIZE)
            {
                maxpa = end;
            }
        }
    }
    
    // 先找到内存最高地址，但不能超过高端内存地址，高端内存通过其它方式来映射
    if (maxpa > KMEMSIZE)
    {
        maxpa = KMEMSIZE;
    }

    // maxpa = 0x1FFE0000
    cprintf("maxpa: %08x\n", maxpa);
    
    // 从内核在内存里结束的地方往后统计
#if ASM_NO_64
    extern char end[];  // 0xC015B384
#else
    char end[1];
#endif
    cprintf("end: %08x\n", end);
    
    // maxpa = 0x1FFE0000 PGSIZE = 0x1000 = 4k
    // 这里 npage 记录的是所有物理内存空间总页数，npage 最大值就是 KMEMSIZE / PGSIZE = 224k
    npage = (size_t)(maxpa / PGSIZE); // 0x0001FFE0
    
    // pages 指向内核代码结束后的第一个空闲 4k 页面，0xC015F384 的下一个 4k 起始是 0xC0160000
    pages = (struct Page *)ROUNDUP((void *)end, PGSIZE);    // 0xC0160000
    cprintf("npage: %08x, pages: %08x\n", npage, pages);
    
    // 代码走到这里，虚拟地址映射还是采用的 boot_pgdir，
    // 虚拟地址 0xC0000000 ~ 0xC1000000 映射到物理地址 0 ~ 16M，并没有按完整的 1G 内核空间映射
    // 目前 qemu 缺省是按照 512M 来虚拟内存大小，此时映射的整个页表空间不超过 16M，没问题
    struct Page *page;
    for (i = 0; i < npage; i++)
    {
        page = &pages[i];
        SetPageReserved(page);
    }
    
    // 真正的内存空闲起始物理地址，除了减去内核占用空间之外，还要减去页表本身占用的空间
    uintptr_t freemem = PADDR((uintptr_t)pages + sizeof(struct Page) * npage);  // 0x005DFB80
    cprintf("freemem: %08x.\n", freemem);

    for (i = 0; i < memmap->nr_map; i++)
    {
        uint64_t begin = memmap->map[i].addr, end = begin + memmap->map[i].size;
        if (memmap->map[i].type == E820_ARM)
        {
            if (begin < freemem)
            {
                begin = freemem;
            }
            if (end > KMEMSIZE)
            {
                end = KMEMSIZE;
            }
            if (begin < end)
            {
                begin = ROUNDUP(begin, PGSIZE);
                end = ROUNDDOWN(end, PGSIZE);
                if (begin < end)
                {
                    cprintf("begin: %08x\n", begin);    // 0x005E0000
                    cprintf("end: %08x\n", end);        // 0x1FFE0000
                    // 从真正空闲的页面开始初始化，出去内核占用，页表本身占用等
                    init_memmap(pa2page((uintptr_t)begin), (size_t)((end - begin) / PGSIZE));
                }
            }
        }
    }
}

//static void enable_paging(void)
//{
//    lcr3(boot_cr3);
//
//    // turn on paging
//    uint32_t cr0 = rcr0();
//    cr0 |= CR0_PE | CR0_PG | CR0_AM | CR0_WP | CR0_NE | CR0_TS | CR0_EM | CR0_MP;
//    cr0 &= ~(CR0_TS | CR0_EM);
//    lcr0(cr0);
//}

//boot_map_segment - setup&enable the paging mechanism
// parameters
//  la:   linear address of this memory need to map (after x86 segment map)
//  size: memory size
//  pa:   physical address of this memory
//  perm: permission of this memory
/*
    linear addr = phy addr + 0xC0000000
    页目录项内容 = (页表起始物理地址 & ~0x0FFF) | PTE_U | PTE_W | PTE_P
    页表项内容 = (pa & ~0x0FFF) | PTE_P | PTE_W
 
    PTE_U：位3，表示用户态的软件可以读取对应地址的物理内存页内容
    PTE_W：位2，表示物理内存页内容可写
    PTE_P：位1，表示物理内存页存在
 
    (gdb) x /1024hx (void *)&__boot_pgdir + 0x300 * 4 页目录下所有页表物理地址，页表地址并不完全连续
    0xC0156c00:    0x7027    0x0015    0xd027    0x0027    0xe027    0x0027    0xf027    0x0027
    0xC0156c10:    0x0027    0x0028    0x1027    0x0028    0x2027    0x0028    0x3027    0x0028
    0xC0156c20:    0x4027    0x0028    0x5027    0x0028    0x6027    0x0028    0x7027    0x0028
    0xC0156c30:    0x8027    0x0028    0x9027    0x0028    0xa027    0x0028    0xb027    0x0028
    0xC0156c40:    0xC027    0x0028    0xd027    0x0028    0xe027    0x0028    0xf027    0x0028
    0xC0156c50:    0x0027    0x0029    0x1027    0x0029    0x2027    0x0029    0x3027    0x0029
    0xC0156c60:    0x4027    0x0029    0x5027    0x0029    0x6027    0x0029    0x7027    0x0029
    0xC0156c70:    0x8027    0x0029    0x9027    0x0029    0xa027    0x0029    0xb027    0x0029
    0xC0156c80:    0xC027    0x0029    0xd027    0x0029    0xe027    0x0029    0xf027    0x0029
    0xC0156c90:    0x0027    0x002a    0x1027    0x002a    0x2027    0x002a    0x3027    0x002a
    0xC0156ca0:    0x4027    0x002a    0x5027    0x002a    0x6027    0x002a    0x7027    0x002a
    0xC0156cb0:    0x8027    0x002a    0x9027    0x002a    0xa027    0x002a    0xb027    0x002a
    0xC0156cc0:    0xC027    0x002a    0xd027    0x002a    0xe027    0x002a    0xf027    0x002a
    0xC0156cd0:    0x0027    0x002b    0x1027    0x002b    0x2027    0x002b    0x3027    0x002b
    0xC0156ce0:    0x4027    0x002b    0x5027    0x002b    0x6027    0x002b    0x7027    0x002b
    0xC0156cf0:    0x8027    0x002b    0x9027    0x002b    0xa027    0x002b    0xb027    0x002b
    0xC0156d00:    0xC027    0x002b    0xd027    0x002b    0xe027    0x002b    0xf027    0x002b
    0xC0156d10:    0x0027    0x002c    0x1027    0x002c    0x2027    0x002c    0x3027    0x002c
    0xC0156d20:    0x4027    0x002c    0x5027    0x002c    0x6027    0x002c    0x7027    0x002c
    0xC0156d30:    0x8027    0x002c    0x9027    0x002c    0xa027    0x002c    0xb027    0x002c
    0xC0156d40:    0xC027    0x002c    0xd027    0x002c    0xe027    0x002c    0xf027    0x002c
    0xC0156d50:    0x0027    0x002d    0x1027    0x002d    0x2027    0x002d    0x3027    0x002d
    0xC0156d60:    0x4027    0x002d    0x5027    0x002d    0x6027    0x002d    0x7027    0x002d
    0xC0156d70:    0x8027    0x002d    0x9027    0x002d    0xa027    0x002d    0xb027    0x002d
    0xC0156d80:    0xC027    0x002d    0xd027    0x002d    0xe027    0x002d    0xf027    0x002d
    0xC0156d90:    0x0027    0x002e    0x1027    0x002e    0x2027    0x002e    0x3027    0x002e
    0xC0156da0:    0x4027    0x002e    0x5027    0x002e    0x6027    0x002e    0x7027    0x002e
    0xC0156db0:    0x8027    0x002e    0x9027    0x002e    0xa027    0x002e    0xb027    0x002e
    0xC0156dc0:    0xC027    0x002e    0xd027    0x002e    0xe027    0x002e    0xf027    0x002e
    0xC0156dd0:    0x0027    0x002f    0x1027    0x002f    0x2027    0x002f    0x3027    0x002f
    0xC0156de0:    0x4027    0x002f    0x5027    0x002f    0x6027    0x002f    0x7027    0x002f
    0xC0156df0:    0x8027    0x002f    0x9027    0x002f    0xa027    0x002f    0xb027    0x002f
    0xC0156e00:    0xC027    0x002f    0xd027    0x002f    0xe027    0x002f    0xf027    0x002f
    0xC0156e10:    0x0027    0x0030    0x1027    0x0030    0x2027    0x0030    0x3027    0x0030
    0xC0156e20:    0x4027    0x0030    0x5027    0x0030    0x6027    0x0030    0x7027    0x0030
    0xC0156e30:    0x8027    0x0030    0x9027    0x0030    0xa027    0x0030    0xb027    0x0030
    0xC0156e40:    0xC027    0x0030    0xd027    0x0030    0xe027    0x0030    0xf027    0x0030
    0xC0156e50:    0x0027    0x0031    0x1027    0x0031    0x2027    0x0031    0x3027    0x0031
    0xC0156e60:    0x4027    0x0031    0x5027    0x0031    0x6027    0x0031    0x7027    0x0031
    0xC0156e70:    0x8027    0x0031    0x9027    0x0031    0xa027    0x0031    0xb027    0x0031
    0xC0156e80:    0xC027    0x0031    0xd027    0x0031    0xe027    0x0031    0xf027    0x0031
    0xC0156e90:    0x0027    0x0032    0x1027    0x0032    0x2027    0x0032    0x3027    0x0032
    0xC0156ea0:    0x4027    0x0032    0x5027    0x0032    0x6027    0x0032    0x7027    0x0032
    0xC0156eb0:    0x8027    0x0032    0x9027    0x0032    0xa027    0x0032    0xb027    0x0032
    0xC0156ec0:    0xC027    0x0032    0xd027    0x0032    0xe027    0x0032    0xf027    0x0032
    0xC0156ed0:    0x0027    0x0033    0x1027    0x0033    0x2027    0x0033    0x3027    0x0033
    0xC0156ee0:    0x4027    0x0033    0x5027    0x0033    0x6027    0x0033    0x7027    0x0033
    0xC0156ef0:    0x8027    0x0033    0x9027    0x0033    0xa027    0x0033    0xb027    0x0033
    0xC0156f00:    0xC027    0x0033    0xd027    0x0033    0xe027    0x0033    0xf027    0x0033
    0xC0156f10:    0x0027    0x0034    0x1027    0x0034    0x2027    0x0034    0x3027    0x0034
    0xC0156f20:    0x4027    0x0034    0x5027    0x0034    0x6027    0x0034    0x7027    0x0034
    0xC0156f30:    0x8027    0x0034    0x9027    0x0034    0xa027    0x0034    0xb027    0x0034
    0xC0156f40:    0xC027    0x0034    0xd027    0x0034    0xe027    0x0034    0xf027    0x0034
    0xC0156f50:    0x0027    0x0035    0x1027    0x0035    0x2027    0x0035    0x3027    0x0035
    0xC0156f60:    0x4027    0x0035    0x5027    0x0035    0x6027    0x0035    0x7027    0x0035
    0xC0156f70:    0x8027    0x0035    0x9027    0x0035    0xa027    0x0035    0xb027    0x0035
    0xC0156f80:    0x0000    0x0000    0x0000    0x0000    0x0000    0x0000    0x0000    0x0000
    0xC0156f90:    0x0000    0x0000    0x0000    0x0000    0x0000    0x0000    0x0000    0x0000
    0xC0156fa0:    0x0000    0x0000    0x0000    0x0000    0x0000    0x0000    0x6003    0x0015
*/
static void boot_map_segment(pde_t *pgdir, uintptr_t la, size_t size, uintptr_t pa, uint32_t perm)
{
    assert(PGOFF(la) == PGOFF(pa));
    size_t n = ROUNDUP(size + PGOFF(la), PGSIZE) / PGSIZE;  // 0x38000
    la = ROUNDDOWN(la, PGSIZE);
    pa = ROUNDDOWN(pa, PGSIZE);
    for (; n > 0; n --, la += PGSIZE, pa += PGSIZE)
    {
        /*
         获取这个虚拟地址在所在的页表地址，如果没有页表就从空闲内存里分配一个空闲 4k 页面当做页表
         第 1 ~ 4 页表 ptep 的虚拟地址是 0xC0157000，页目录起始地址是 0xC0156000，1k个页目录，
         每个四字节，0xC0156000 + 0x400 * 4 = 0xC0157000，这个空间在 entry.S 里就已经分配好了
         第 5 个页表 ptep 的虚拟地址和第一个 ptep 并不连续，是因为从第 5 页表开始，内存空间
         是通过 alloc_page 分配过来的，不是之前规划的
        */
        pte_t *ptep = get_pte(pgdir, la, 1);
        assert(ptep != NULL);
        *ptep = pa | PTE_P | perm;
    }
}

//boot_alloc_page - allocate one page using pmm->alloc_pages(1) 
// return value: the kernel virtual address of this allocated page
//note: this function is used to get the memory for PDT(Page Directory Table)&PT(Page Table)
//static void *boot_alloc_page(void)
//{
//    struct Page *p = alloc_page();
//    if (p == NULL) {
//        panic("boot_alloc_page failed.\n");
//    }
//    return page2kva(p);
//}

//pmm_init - setup a pmm to manage physical memory, build PDT&PT to setup paging mechanism
//         - check the correctness of pmm & paging mechanism, print PDT&PT
void pmm_init(void)
{
    // We've already enabled paging 进入内核之前临时设置的页表
    // 0x156000 = PADDR(0xC0156000)
    boot_cr3 = PADDR(boot_pgdir);

    //We need to alloc/free the physical memory (granularity is 4KB or other size). 
    //So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    //First we should init a physical memory manager(pmm) based on the framework.
    //Then pmm can alloc/free the physical memory. 
    //Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    static_assert(KERNBASE % PTSIZE == 0 && KERNTOP % PTSIZE == 0);

    // recursively insert boot_pgdir in itself
    // to form a virtual page table at virtual address VPT
    // boot_pgdir[0x3EB] = 0x00156000 | PTE_P | PTE_W = 0x00156003
    boot_pgdir[PDX(VPT)] = PADDR(boot_pgdir) | PTE_P | PTE_W;

    // map all physical memory to linear memory with base linear addr KERNBASE
    // linear_addr KERNBASE ~ KERNBASE + KMEMSIZE = phy_addr 0 ~ KMEMSIZE
    // 映射虚拟地址 0xC0000000 ~ 0xF8000000 到物理地址 0 ~ 0x38000000
    // 这段代码其实可以不执行，虚拟地址映射全部挪到 entry.S 里提前映射完，
    // 防止后续代码访问到未映射的地址
    boot_map_segment(boot_pgdir, KERNBASE, KMEMSIZE, 0, PTE_W);

    // Since we are using bootloader's GDT,
    // we should reload gdt (second time, the last time) to get user segments and the TSS
    // map virtual_addr 0 ~ 4G = linear_addr 0 ~ 4G
    // then set kernel stack (ss:esp) in TSS, setup TSS in gdt, load TSS
    // 这里 gdt 只是给 BSP cpu 设置，AP cpu 后续单独设置
    gdt_init();

    //use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();
    
    check_pgdir();
    check_boot_pgdir();
    
    print_pgdir();
    
    slab_init();
}

//get_pte - get pte and return the kernel virtual address of this pte for la
//        - if the PT contians this pte didn't exist, alloc a page for PT
// parameter:
//  pgdir:  the kernel virtual base address of PDT
//  la:     the linear address need to map
//  create: a logical value to decide if alloc a page for PT
// return vaule: the kernel virtual address of this pte
/*
 最重要的一点就是要明白页目录和页表中存储的都是物理地址。
 所以当我们从页目录中获取页表的物理地址后，我们需要使用KADDR()将其转换成虚拟地址。
 之后就可以用这个虚拟地址直接访问对应的页表了
 
 给定一个页目录，给定一个虚拟地址，找出这个虚拟地址在所在的页表物理地址。
 通过更改此项的值可以将虚拟地址映射到另外的页上。
 pgdir 给出页表起始地址。通过查找这个页表，我们需要给出二级页表中对应项的地址。
 虽然目前我们只有 boot_pgdir 一个页表，但是引入进程的概念之后每个进程都会有自己的页表
 
 有可能根本就没有对应的二级页表的情况，所以二级页表不必要一开始就分配，而是等到需要的时候再添加对应的二级页表。
 如果在查找二级页表项时，发现对应的二级页表不存在，则需要根据 create 参数的值来处理是否创建新的二级页表。
 如果 create 参数为 0，则 get_pte 返回 NULL；如果 create 参数不为 0，则 get_pte 需要申请一个新的物理页
 （通过 alloc_page 来实现，可在mm/pmm.h中找到它的定义），再在一级页表中添加页目录项指向表示二级页表的新物理页。
 注意，新申请的页必须全部设定为零，因为这个页所代表的虚拟地址都没有被映射。
 
 只有当一级二级页表的项都设置了用户写权限后，用户才能对对应的物理地址进行读写。
 所以我们可以在一级页表先给用户写权限，再在二级页表上面根据需要限制用户的权限，对物理页进行保护。
 由于一个物理页可能被映射到不同的虚拟地址上去（譬如一块内存在不同进程间共享）
*/
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    // 取虚拟地址页目录地址
    pde_t *pdep = &pgdir[PDX(la)];
    if (!(*pdep & PTE_P))
    {
        // 如果还没分配物理页，根据 create 字段来判断是否分配物理页，有可能只是单纯的查询，不需要创建页表
        struct Page *page = NULL;
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        // 页目录都给了 PTE_U 用户权限，页表再来差异控制访问
        *pdep = pa | PTE_U | PTE_W | PTE_P;
    }
    
    // 如果已经分配物理页，直接返回虚拟地址对应的物理页表
    pte_t *ptep = &((pte_t *)KADDR(PDE_ADDR(*pdep)))[PTX(la)];
    return ptep;
}

//get_page - get related Page struct for linear address la using PDT pgdir
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep_store != NULL)
    {
        *ptep_store = ptep;
    }
    if (ptep != NULL && *ptep & PTE_P)
    {
        return pte2page(*ptep);
    }
    return NULL;
}

//page_remove_pte - free an Page sturct which is related linear address la
//                - and clean(invalidate) pte which is related linear address la
//note: PT is changed, so the TLB need to be invalidate 
static inline void page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep)
{
    if (*ptep & PTE_P)
    {
        struct Page *page = pte2page(*ptep);
        // 当这个物理页引用计数为 0 的时候才能回收页面，有可能多个虚拟地址映射到同一个物理页面
        // 这样多对一的映射是为了做内存共享
        if (page_ref_dec(page) == 0)
        {
            free_page(page);
        }
        *ptep = 0;
        tlb_invalidate(pgdir, la);
    }
}

void unmap_range(pde_t *pgdir, uintptr_t start, uintptr_t end)
{
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));

    do {
        pte_t *ptep = get_pte(pgdir, start, 0);
        if (ptep == NULL)
        {
            start = ROUNDDOWN(start + PTSIZE, PTSIZE);
            continue ;
        }
        if (*ptep != 0)
        {
            page_remove_pte(pgdir, start, ptep);
        }
        start += PGSIZE;
    } while (start != 0 && start < end);
}

void exit_range(pde_t *pgdir, uintptr_t start, uintptr_t end)
{
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));

    start = ROUNDDOWN(start, PTSIZE);
    do {
        int pde_idx = PDX(start);
        if (pgdir[pde_idx] & PTE_P)
        {
            free_page(pde2page(pgdir[pde_idx]));
            pgdir[pde_idx] = 0;
        }
        start += PTSIZE;
    } while (start != 0 && start < end);
}

/* copy_range - copy content of memory (start, end) of one process A to another process B
 * @to:    the addr of process B's Page Directory
 * @from:  the addr of process A's Page Directory
 * @share: flags to indicate to dup OR share. We just use dup method, so it didn't be used.
 *
 * CALL GRAPH: copy_mm-->dup_mmap-->copy_range
 * 实现步骤:
 *          1.为 to 分配一个 Page 对象 npage
 *          2.得到 start 所在 from 的 pte, 相应的 page
 *          3.得到 npage 的物理内存在 kernel 中的 kva_det
 *          4.得到 page 的物理内存在 kernel 中的 kva_src
 *          5.在 ring0 状态拷贝 kva_src 到 kva_det
 *          6.设置 to 的 npage 的相应的 pte
 * do_fork --> copy_mm --> dup_mmap --> copy_range
 */
// copy_range 函数就是调用一个 memcpy 将父进程的内存直接复制给子进程
int copy_range(struct mm_struct *to, struct mm_struct *from, uintptr_t start, uintptr_t end, bool share, bool useCOW)
{
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));

    do
    {
        //call get_pte to find process A's pte according to the addr start
        pte_t *ptep = get_pte(from->pgdir, start, 0), *nptep;
        if (ptep == NULL)
        {
            start = ROUNDDOWN(start + PTSIZE, PTSIZE);
            continue;
        }
        //call get_pte to find process B's pte according to the addr start. If pte is NULL, just alloc a PT
        if (*ptep & PTE_P)
        {
            // 进入这里，说明 from 页表里某些虚拟地址已经分配好物理页面
            // 这部分物理页面需要根据情况来区分处理
            if ((nptep = get_pte(to->pgdir, start, 1)) == NULL)
            {
                return -E_NO_MEM;
            }
            uint32_t perm = (*ptep & PTE_USER);
            //get page from ptep
            struct Page *page = pte2page(*ptep);
            assert(page != NULL);
            int ret = 0;
            check_mm_struct = NULL;
            if (share)
            {
                // 如果是共享内存，from 和 to 共享同一个物理页面
                // 这里直接把 from 的共享物理页面插入 to 页表里
                ret = page_insert(to->pgdir, page, start, perm);
                assert(ret == 0);
            }
            else
            {
                if (useCOW)
                {
                    // 如果使用 COW 机制，先把 from 所在的可写物理页面全部标记为只读
                    // 后续需要写内存的时候触发 page fault，再申请新内存页面进行内存修改
                    // 这样不用一开始申请内存，用到了再申请，节省资源
                    if (*ptep & PTE_W)
                    {
                        perm &= (~PTE_W);
                    }
                    
                    // 建立子进程页地址起始位置与物理地址的映射关系(prem是权限)
                    ret = page_insert(to->pgdir, page, start, perm);
                    assert(ret == 0);
                }
                else
                {
                    // 如果不使用 COW 机制，就直接申请物理内存页面，从 from 那里拷贝数据
                    // 但这会浪费内存，因为子进程大部分会读取父进程的数据，需要改动的页面很少
                    struct Page *npage = alloc_page();
                    assert(npage != NULL);
                    
                    void *kva_src = page2kva(page);
                    void *kva_dst = page2kva(npage);
                    
                    // 复制 from 的物理页面内容给 to
                    memcpy(kva_dst, kva_src, PGSIZE);
                    
                    // 建立子进程页地址起始位置与物理地址的映射关系(prem是权限)
                    ret = page_insert(to->pgdir, npage, start, perm);
                    assert(ret == 0);
                }
            }
        }
        start += PGSIZE;
    } while (start != 0 && start < end);
    
    return 0;
}

//page_remove - free an Page which is related linear address la and has an validated pte
void page_remove(pde_t *pgdir, uintptr_t la)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep != NULL)
    {
        page_remove_pte(pgdir, la, ptep);
    }
}

//page_insert - build the map of phy addr of an Page with the linear addr la
// paramemters:
//  pgdir: the kernel virtual base address of PDT
//  page:  the Page which need to map
//  la:    the linear address need to map
//  perm:  the permission of this Page which is setted in related pte
// return value: always 0
//note: PT is changed, so the TLB need to be invalidate
// 将物理页映射在了页表上，如果没有页表就分配
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)
{
    // 获取这个虚拟地址在所在的页表地址，如果没有页表就分配
    pte_t *ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL)
    {
        return -E_NO_MEM;
    }
    
    page_ref_inc(page);
    if (*ptep & PTE_P) // 当前页面已经被其它虚拟地址映射了
    {
        struct Page *p = pte2page(*ptep);
        if (p == page)
        {
            // 如果是同一个虚拟地址在映射，就减去刚才增加的引用计数
            page_ref_dec(page);
        }
        else
        {
            page_remove_pte(pgdir, la, ptep);
        }
    }
    // 标记当前物理页面已经被映射了
    *ptep = page2pa(page) | PTE_P | perm;
    // 加入快表，提高 CPU 后续转换地址的效率
    tlb_invalidate(pgdir, la);
    return 0;
}

// invalidate a TLB entry, but only if the page tables being
// edited are the ones currently in use by the processor.
/*
 处理器使用快表 TLB（Translation Lookaside Buffer）来缓存线性地址到物理地址的映射关系。
 实际的地址转换过程中，处理器首先根据线性地址查找TLB，如果未发现该线性地址到物理地址的映射关
 系（TLB miss），将根据页表中的映射关系填充TLB（TLB fill），然后再进行地址转换。
 这样可以提高线性地址转换到物理地址的效率。
*/
void tlb_invalidate(pde_t *pgdir, uintptr_t la)
{
    if (rcr3() == PADDR(pgdir))
    {
        invlpg((void *)la);
    }
}

//
// Reserve size bytes in the MMIO region and map [pa,pa+size] at this
// location.  Return the base of the reserved region.   size does *not*
// have to be multiple of PGSIZE.
//
void *mmio_map_region(physaddr_t pa, size_t size)
{
    // Where to start the next region. Initially, this is the
    // begining of the MMIO region. Because this is static, its
    // value will be preserved between calls to mmio_map_region
    // (just like nextfree in boot_alloc).
    static uintptr_t base = MMIOBASE;

    size = (size_t)ROUNDUP(size, PGSIZE);
    if (base + size > MMIOLIM)
    {
        panic("mmio_map_region: not enough memory"); 
    }

    boot_map_segment(boot_pgdir, base, size, pa, PTE_W | PTE_PCD | PTE_PWT | PTE_P);
    base +=size;

    return (void *)(base - size);
}


// pgdir_alloc_page - call alloc_page & page_insert functions to
//                  - allocate a page size memory & setup an addr map
//                  - pa<->la with linear address la and the PDT pgdir
struct Page *pgdir_alloc_page(struct mm_struct *mm, uintptr_t la, uint32_t perm)
{
    pde_t *pgdir = mm->pgdir;
    struct Page *page = alloc_page();
    if (page != NULL)
    {
        if (page_insert(pgdir, page, la, perm) != 0)
        {
            free_page(page);
            return NULL;
        }
        
        if (swap_init_ok)
        {
            swap_map_swappable(mm, la, page, 0);
            page->pra_vaddr = la;
            assert(page_ref(page) == 1);
        }
    }

    return page;
}

static void check_alloc_page(void)
{
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

static void check_pgdir(void)
{
    assert(npage <= KMEMSIZE / PGSIZE);
    assert(boot_pgdir != NULL && (uint32_t)PGOFF(boot_pgdir) == 0);
    assert(get_page(boot_pgdir, 0x0, NULL) == NULL);

    struct Page *p1, *p2;
    p1 = alloc_page();
    assert(page_insert(boot_pgdir, p1, 0x0, 0) == 0);

    pte_t *ptep;
    assert((ptep = get_pte(boot_pgdir, 0x0, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert(page_ref(p1) == 1);

    ptep = &((pte_t *)KADDR(PDE_ADDR(boot_pgdir[0])))[1];
    assert(get_pte(boot_pgdir, PGSIZE, 0) == ptep);

    p2 = alloc_page();
    assert(page_insert(boot_pgdir, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert((ptep = get_pte(boot_pgdir, PGSIZE, 0)) != NULL);
    assert(*ptep & PTE_U);
    assert(*ptep & PTE_W);
    assert(boot_pgdir[0] & PTE_U);
    assert(page_ref(p2) == 1);

    assert(page_insert(boot_pgdir, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2);
    assert(page_ref(p2) == 0);
    assert((ptep = get_pte(boot_pgdir, PGSIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert((*ptep & PTE_U) == 0);

    page_remove(boot_pgdir, 0x0);
    assert(page_ref(p1) == 1);
    assert(page_ref(p2) == 0);

    page_remove(boot_pgdir, PGSIZE);
    assert(page_ref(p1) == 0);
    assert(page_ref(p2) == 0);

    assert(page_ref(pde2page(boot_pgdir[0])) == 1);
    free_page(pde2page(boot_pgdir[0]));
    boot_pgdir[0] = 0;

    cprintf("check_pgdir() succeeded!\n");
}

static void check_boot_pgdir(void)
{
    pte_t *ptep;
    int i;
    for (i = 0; i < npage; i += PGSIZE)
    {
        assert((ptep = get_pte(boot_pgdir, (uintptr_t)KADDR(i), 0)) != NULL);
        assert(PTE_ADDR(*ptep) == i);
    }

    assert(PDE_ADDR(boot_pgdir[PDX(VPT)]) == PADDR(boot_pgdir));
    assert(boot_pgdir[0] == 0);
    
    struct Page *p;
    p = alloc_page();
    assert(page_insert(boot_pgdir, p, 0x100, PTE_W) == 0);
    assert(page_ref(p) == 1);
    assert(page_insert(boot_pgdir, p, 0x100 + PGSIZE, PTE_W) == 0);
    assert(page_ref(p) == 2);

    const char *str = "ucore: Hello world!!";
    strcpy((void *)0x100, str);
    assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0);

    *(char *)(page2kva(p) + 0x100) = '\0';
    assert(strlen((const char *)0x100) == 0);

    free_page(p);
    free_page(pde2page(boot_pgdir[0]));
    boot_pgdir[0] = 0;

    cprintf("check_boot_pgdir() succeeded!\n");
}

//perm2str - use string 'u,r,w,-' to present the permission
static const char *perm2str(int perm)
{
    static char str[4];
    str[0] = (perm & PTE_U) ? 'u' : '-';
    str[1] = 'r';
    str[2] = (perm & PTE_W) ? 'w' : '-';
    str[3] = '\0';
    return str;
}

//get_pgtable_items - In [left, right] range of PDT or PT, find a continuous linear addr space
//                  - (left_store*X_SIZE~right_store*X_SIZE) for PDT or PT
//                  - X_SIZE=PTSIZE=4M, if PDT; X_SIZE=PGSIZE=4K, if PT
// paramemters:
//  left:        no use ???
//  right:       the high side of table's range
//  start:       the low side of table's range
//  table:       the beginning addr of table
//  left_store:  the pointer of the high side of table's next range
//  right_store: the pointer of the low side of table's next range
// return value: 0 - not a invalid item range, perm - a valid item range with perm permission 
static int get_pgtable_items(size_t left, size_t right, size_t start, uintptr_t *table, size_t *left_store, size_t *right_store)
{
    if (start >= right) {
        return 0;
    }
    while (start < right && !(table[start] & PTE_P)) {
        start ++;
    }
    if (start < right) {
        if (left_store != NULL) {
            *left_store = start;
        }
        int perm = (table[start ++] & PTE_USER);
        while (start < right && (table[start] & PTE_USER) == perm) {
            start ++;
        }
        if (right_store != NULL) {
            *right_store = start;
        }
        return perm;
    }
    return 0;
}

//print_pgdir - print the PDT&PT
/*
    -------------------- BEGIN --------------------
    PDE(0e0)        c0000000-f8000000 38000000 urw
    |-- PTE(38000)  c0000000-f8000000 38000000 -rw
    PDE(001)        fac00000-fb000000 00400000 -rw
    |-- PTE(000e0)  faf00000-fafe0000 000e0000 urw
    |-- PTE(00001)  fafeb000-fafec000 00001000 -rw
    --------------------- END ---------------------
 */
void print_pgdir(void)
{
    cprintf("\n-------------------- print_pgdir BEGIN --------------------\n");
    if (current)
    {
        cprintf("print_pgdir: pid = %d, name = \"%s\", runs = %d.\n", current->pid, current->name, current->runs);
    }
    size_t left, right = 0, perm;
    while ((perm = get_pgtable_items(0, NPDEENTRY, right, vpd, &left, &right)) != 0)
    {
        cprintf("PDE(%03x) %08x-%08x %08x %s\n", right - left, left * PTSIZE, right * PTSIZE, (right - left) * PTSIZE, perm2str(perm));
        size_t l, r = left * NPTEENTRY;
        while ((perm = get_pgtable_items(left * NPTEENTRY, right * NPTEENTRY, r, vpt, &l, &r)) != 0)
        {
            cprintf("  |-- PTE(%05x) %08x-%08x %08x %s\n", r - l, l * PGSIZE, r * PGSIZE, (r - l) * PGSIZE, perm2str(perm));
        }
    }
    cprintf("--------------------- print_pgdir END ---------------------\n");
    if (current)
        dump_vma(current->mm);
}

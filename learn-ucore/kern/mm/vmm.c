#include "vmm.h"
#include "sync.h"
#include "string.h"
#include "assert.h"
#include "stdio.h"
#include "error.h"
#include "pmm.h"
#include "x86.h"
#include "swap.h"
#include "slab.h"

/* 
  vmm design include two parts: mm_struct (mm) & vma_struct (vma)
  mm is the memory manager for the set of continuous virtual memory  
  area which have the same PDT. vma is a continuous virtual memory area.
  There a linear link list for vma & a redblack link list for vma in mm.
---------------
  mm related functions:
   golbal functions
     struct mm_struct * mm_create(void)
     void mm_destroy(struct mm_struct *mm)
     int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr)
--------------
  vma related functions:
   global functions
     struct vma_struct * vma_create (uintptr_t vm_start, uintptr_t vm_end,...)
     void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma)
     struct vma_struct * find_vma(struct mm_struct *mm, uintptr_t addr)
   local functions
     inline void check_vma_overlap(struct vma_struct *prev, struct vma_struct *next)
---------------
   check correctness functions
     void check_vmm(void);
     void check_vma_struct(void);
     void check_pgfault(void);
*/

static void check_vmm(void);
static void check_vma_struct(void);
static void check_pgfault(void);

// mm_create -  alloc a mm_struct & initialize it.
struct mm_struct *mm_create(void)
{
    struct mm_struct *mm = (struct mm_struct *)kmalloc(sizeof(struct mm_struct));
    if (mm != NULL)
    {
        list_init(&(mm->mmap_list));
        mm->mmap_cache = NULL;
        mm->pgdir = NULL;
        mm->map_count = 0;

        if (swap_init_ok)
            swap_init_mm(mm);
        else
            mm->sm_priv = NULL;
        
        set_mm_count(mm, 0);
        mm->locked_by = 0;
        mm->brk_start = 0;
        mm->brk = 0;
        sem_init(&(mm->mm_sem), 1);
    }    
    return mm;
}

// vma_create - alloc a vma_struct & initialize it. (addr range: vm_start~vm_end)
struct vma_struct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags)
{
    struct vma_struct *vma = (struct vma_struct *)kmalloc(sizeof(struct vma_struct));
    if (vma != NULL)
    {
        vma->vm_start = vm_start;
        vma->vm_end = vm_end;
        vma->vm_flags = vm_flags;
    }
    return vma;
}

// vma_destroy - free vma_struct
static void vma_destroy(struct vma_struct *vma)
{
    if (vma->vm_flags & VM_SHARE)
    {
//        if (shmem_ref_dec(vma->shmem) == 0)
//        {
//            shmem_destroy(vma->shmem);
//        }
    }
    kfree(vma);
}

static void vma_resize(struct vma_struct *vma, uintptr_t start, uintptr_t end)
{
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(vma->vm_start <= start && start < end && end <= vma->vm_end);
    if (vma->vm_flags & VM_SHARE)
    {
//        vma->shmem_off += start - vma->vm_start;
    }
#ifdef UCONFIG_BIONIC_LIBC
    if (vma->mfile.file != NULL)
    {
        vma->mfile.offset += start - vma->vm_start;
    }
#endif //UCONFIG_BIONIC_LIBC
    
    vma->vm_start = start, vma->vm_end = end;
}

// find_vma - find a vma  (vma->vm_start <= addr <= vma_vm_end)
// 根据输入参数 addr 和 mm 变量，查找在 mm 变量中的 mmap_list 双向链表中某个 vma 包含此 addr，
// 即 vma->vm_start <= addr end
struct vma_struct *find_vma(struct mm_struct *mm, uintptr_t addr)
{
    struct vma_struct *vma = NULL;
    if (mm != NULL)
    {
        vma = mm->mmap_cache;
        if (!(vma != NULL && vma->vm_start <= addr && vma->vm_end > addr))
        {
            bool found = 0;
            list_entry_t *list = &(mm->mmap_list), *le = list;
            while ((le = list_next(le)) != list)
            {
                vma = le2vma(le, list_link);
                if (vma->vm_start <= addr && addr < vma->vm_end)
                {
                    found = 1;
                    break;
                }
            }
            if (!found)
            {
                vma = NULL;
            }
        }
        if (vma != NULL)
        {
            // 每次查找都更新下缓存，方便下次查找，提高效率
            mm->mmap_cache = vma;
        }
    }
    return vma;
}

/* Look up the first VMA which intersects the interval start_addr..end_addr-1,
 NULL if none.  Assume start_addr < end_addr. */
struct vma_struct *find_vma_intersection(struct mm_struct *mm, uintptr_t start, uintptr_t end)
{
    struct vma_struct *vma = find_vma(mm, start);
    if (vma && end <= vma->vm_start)
        vma = NULL;
    
    return vma;
}

// check_vma_overlap - check if vma1 overlaps vma2 ?
static inline void check_vma_overlap(struct vma_struct *prev, struct vma_struct *next)
{
    assert(prev->vm_start < prev->vm_end);
    assert(prev->vm_end <= next->vm_start);
    assert(next->vm_start < next->vm_end);
}

// insert_vma_struct -insert vma in mm's list link
// 把一个 vma 变量按照其空间位置 [vma->vm_start, vma->vm_end] 从小到大的顺序插入到
// 所属的 mm 变量中的 mmap_list 双向链表中
void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma)
{
    assert(vma->vm_start < vma->vm_end);
    list_entry_t *list = &(mm->mmap_list);
    list_entry_t *le_prev = list, *le_next;

    list_entry_t *le = list;
    while ((le = list_next(le)) != list)
    {
        struct vma_struct *mmap_prev = le2vma(le, list_link);
        if (mmap_prev->vm_start > vma->vm_start)
        {
            break;
        }
        le_prev = le;
    }

    le_next = list_next(le_prev);

    /* check overlap */
    if (le_prev != list)
    {
        check_vma_overlap(le2vma(le_prev, list_link), vma);
    }
    if (le_next != list)
    {
        check_vma_overlap(vma, le2vma(le_next, list_link));
    }

    vma->vm_mm = mm;
    list_add_after(le_prev, &(vma->list_link));

    mm->map_count++;
}

// remove_vma_struct - remove vma from mm's rb tree link & list link
static int remove_vma_struct(struct mm_struct *mm, struct vma_struct *vma)
{
    assert(mm == vma->vm_mm);
//    if (mm->mmap_tree != NULL)
//    {
//        rb_delete(mm->mmap_tree, &(vma->rb_link));
//    }
    list_del(&(vma->list_link));
    if (vma == mm->mmap_cache)
    {
        mm->mmap_cache = NULL;
    }
    mm->map_count--;
    return 0;
}

void dump_vma(struct mm_struct *mm)
{
    struct vma_struct *vma = NULL;
    if (mm != NULL)
    {
        cprintf("dump_vma start ================================\n");
        list_entry_t *list = &(mm->mmap_list), *le = list;
        while ((le = list_next(le)) != list)
        {
            vma = le2vma(le, list_link);
            
            int8_t flagstr[100] = {0};
            if (vma->vm_flags & VM_READ)
            {
                strcat(flagstr, " read");
            }

            if (vma->vm_flags & VM_WRITE)
            {
                strcat(flagstr, " write");
            }

            if (vma->vm_flags & VM_EXEC)
            {
                strcat(flagstr, " exec");
            }

            if (vma->vm_flags & VM_STACK)
            {
                strcat(flagstr, " stack");
            }

            if (vma->vm_flags & VM_SHARE)
            {
                strcat(flagstr, " share");
            }
            
            cprintf("vm_start:0x%x, vm_end:0x%x, vm_length:0x%x, vm_flags:%s\n", vma->vm_start, vma->vm_end, vma->vm_end - vma->vm_start, flagstr);
        }
        cprintf("dump_vma end ================================\n");
    }
}

// mm_destroy - free mm and mm internal fields
void mm_destroy(struct mm_struct *mm)
{
    assert(mm_count(mm) == 0);

    list_entry_t *list = &(mm->mmap_list), *le;
    while ((le = list_next(list)) != list)
    {
        list_del(le);
        kfree(le2vma(le, list_link));  //kfree vma        
    }
    kfree(mm); //kfree mm
    mm = NULL;
}

// 这里只是分配虚拟页表结构，并未实际分配对应的物理内存，物理内存等到缺页中断的时候再分配
int mm_map(struct mm_struct *mm, uintptr_t addr, size_t len, uint32_t vm_flags, struct vma_struct **vma_store)
{
    uintptr_t start = ROUNDDOWN(addr, PGSIZE), end = ROUNDUP(addr + len, PGSIZE);
    if (!USER_ACCESS(start, end))
    {
        return -E_INVAL;
    }

    assert(mm != NULL);

    int ret = -E_INVAL;

    struct vma_struct *vma;
    if ((vma = find_vma(mm, start)) != NULL && end > vma->vm_start)
    {
        goto out;
    }
    ret = -E_NO_MEM;

    if ((vma = vma_create(start, end, vm_flags)) == NULL)
    {
        goto out;
    }
    insert_vma_struct(mm, vma);
    if (vma_store != NULL)
    {
        *vma_store = vma;
    }
    ret = 0;

out:
    return ret;
}

// 这里不仅仅是释放虚拟页表结构，页表对应的物理内存页可能也已经分配，也需要释放
int mm_unmap(struct mm_struct *mm, uintptr_t addr, size_t len)
{
    uintptr_t start = ROUNDDOWN(addr, PGSIZE), end = ROUNDUP(addr + len, PGSIZE);
    if (!USER_ACCESS(start, end))
    {
        return -E_INVAL;
    }
    
    assert(mm != NULL);
    
    struct vma_struct *vma;
    if ((vma = find_vma(mm, start)) == NULL || end <= vma->vm_start)
    {
        return 0;
    }
    
    if (vma->vm_start < start && end < vma->vm_end)
    {
        // 进入这里说明需要释放的区域是一个页表中间的地方，此时页表要拆分
        struct vma_struct *nvma;
        if ((nvma = vma_create(vma->vm_start, start, vma->vm_flags)) == NULL)
        {
            return -E_NO_MEM;
        }
#ifdef UCONFIG_BIONIC_LIBC
        vma_copymapfile(nvma, vma);
#endif //UCONFIG_BIONIC_LIBC
        vma_resize(vma, end, vma->vm_end);
        insert_vma_struct(mm, nvma);
        unmap_range(mm->pgdir, start, end);
        return 0;
    }
    
    // 走到这里，说明需要释放的空间包含了多个 vma 结构
    // 先新建一个 free_list 把这些这 vma 找到并保存起来，后续释放
    list_entry_t free_list, *le;
    list_init(&free_list);
    while (vma->vm_start < end)
    {
        le = list_next(&(vma->list_link));
        remove_vma_struct(mm, vma);
        list_add(&free_list, &(vma->list_link));
        if (le == &(mm->mmap_list))
        {
            break;
        }
        vma = le2vma(le, list_link);
    }
    
    le = list_next(&free_list);
    while (le != &free_list)
    {
        vma = le2vma(le, list_link);
        le = list_next(le);
        uintptr_t un_start, un_end;
        if (vma->vm_start < start)
        {
            un_start = start;
            un_end = vma->vm_end;
            vma_resize(vma, vma->vm_start, un_start);
            insert_vma_struct(mm, vma);
        }
        else
        {
            un_start = vma->vm_start;
            un_end = vma->vm_end;
            if (end < un_end)
            {
                un_end = end;
                vma_resize(vma, un_end, vma->vm_end);
                insert_vma_struct(mm, vma);
            }
            else
            {
#ifdef UCONFIG_BIONIC_LIBC
                vma_unmapfile(vma);
#endif //UCONFIG_BIONIC_LIBC
                vma_destroy(vma);
            }
        }
        unmap_range(mm->pgdir, un_start, un_end);
    }
    return 0;
}

/*
 创建子进程的函数 do_fork 在执行中将拷贝当前进程（即父进程）的用户内存地址空间中的合法
 内容到新进程中（子进程），完成内存资源的复制。具体是通过 copy_range 函数
 
 如何实现 COW 快照（Copy-On-Write）
 在创建子进程时，将父进程的PDE直接赋值给子进程的 PDE，但是需要将允许写入的标志位置 0；
 当子进程需要进行写操作时，再次出发中断调用 do_pgfault()，此时应给子进程新建 PTE，
 并取代原先 PDE 中的项，然后才能写入。
*/
int dup_mmap(struct mm_struct *to, struct mm_struct *from)
{
    assert(to != NULL && from != NULL);
    list_entry_t *list = &(from->mmap_list), *le = list;
    while ((le = list_prev(le)) != list)
    {
        struct vma_struct *vma, *nvma;
        vma = le2vma(le, list_link);
        nvma = vma_create(vma->vm_start, vma->vm_end, vma->vm_flags);
        if (nvma == NULL)
        {
            return -E_NO_MEM;
        }

        insert_vma_struct(to, nvma);

        bool share = 0;
        if (copy_range(to, from, vma->vm_start, vma->vm_end, share) != 0)
        {
            return -E_NO_MEM;
        }
    }
    return 0;
}

void exit_mmap(struct mm_struct *mm)
{
    assert(mm != NULL && mm_count(mm) == 0);
    pde_t *pgdir = mm->pgdir;
    list_entry_t *list = &(mm->mmap_list), *le = list;
    while ((le = list_next(le)) != list)
    {
        struct vma_struct *vma = le2vma(le, list_link);
        unmap_range(pgdir, vma->vm_start, vma->vm_end);
    }
    while ((le = list_next(le)) != list)
    {
        struct vma_struct *vma = le2vma(le, list_link);
        exit_range(pgdir, vma->vm_start, vma->vm_end);
    }
}

/*
 寻找没有映射的虚拟地址空间，寻找方式是从用户空间的最高端地址，往最低端地址方向(list_prev)
 一个个的扫描目前已经存在的 vma 结构，直到找到不重叠的空闲的连续区域
*/
uintptr_t get_unmapped_area(struct mm_struct *mm, size_t len)
{
    if (len == 0 || len > USERTOP)
    {
        return 0;
    }
    uintptr_t start = USERTOP - len;
    list_entry_t *list = &(mm->mmap_list), *le = list;
    while ((le = list_prev(le)) != list)
    {
        struct vma_struct *vma = le2vma(le, list_link);
        if (start >= vma->vm_end)
        {
            return start;
        }
        if (start + len > vma->vm_start)
        {
            // 进入这里说明找到的虚拟地址和现有 vma 有重叠，需要往下找下一个 vma 继续对比
            if (len >= vma->vm_start)
            {
                return 0;
            }
            start = vma->vm_start - len;
        }
    }
    return (start >= USERBASE) ? start : 0;
}

bool copy_from_user(struct mm_struct *mm, void *dst, const void *src, size_t len, bool writable)
{
    if (!user_mem_check(mm, (uintptr_t)src, len, writable))
    {
        return 0;
    }
    memcpy(dst, src, len);
    return 1;
}

bool copy_to_user(struct mm_struct *mm, void *dst, const void *src, size_t len)
{
    if (!user_mem_check(mm, (uintptr_t)dst, len, 1))
    {
        return 0;
    }
    memcpy(dst, src, len);
    return 1;
}

// vmm_init - initialize virtual memory management
//          - now just call check_vmm to check correctness of vmm
void vmm_init(void)
{
    check_vmm();
}

// check_vmm - check correctness of vmm
static void check_vmm(void)
{
//    size_t nr_free_pages_store = nr_free_pages();
    
    check_vma_struct();
    check_pgfault();

    //assert(nr_free_pages_store == nr_free_pages());

    cprintf("check_vmm() succeeded.\n");
}

static void check_vma_struct(void)
{
//    size_t nr_free_pages_store = nr_free_pages();

    struct mm_struct *mm = mm_create();
    assert(mm != NULL);

    int step1 = 10, step2 = step1 * 10;

    int i = 0;
    for (i = step1; i >= 1; i--)
    {
        struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        insert_vma_struct(mm, vma);
    }

    for (i = step1 + 1; i <= step2; i ++)
    {
        struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        insert_vma_struct(mm, vma);
    }

    list_entry_t *le = list_next(&(mm->mmap_list));

    for (i = 1; i <= step2; i ++)
    {
        assert(le != &(mm->mmap_list));
        struct vma_struct *mmap = le2vma(le, list_link);
        assert(mmap->vm_start == i * 5 && mmap->vm_end == i * 5 + 2);
        le = list_next(le);
    }

    for (i = 5; i <= 5 * step2; i += 5)
    {
        struct vma_struct *vma1 = find_vma(mm, i);
        assert(vma1 != NULL);
        struct vma_struct *vma2 = find_vma(mm, i + 1);
        assert(vma2 != NULL);
        struct vma_struct *vma3 = find_vma(mm, i + 2);
        assert(vma3 == NULL);
        struct vma_struct *vma4 = find_vma(mm, i + 3);
        assert(vma4 == NULL);
        struct vma_struct *vma5 = find_vma(mm, i + 4);
        assert(vma5 == NULL);

        assert(vma1->vm_start == i && vma1->vm_end == i + 2);
        assert(vma2->vm_start == i && vma2->vm_end == i + 2);
    }

    for (i = 4; i >= 0; i--)
    {
        struct vma_struct *vma_below_5 = find_vma(mm, i);
        if (vma_below_5 != NULL)
        {
            cprintf("vma_below_5: i %x, start %x, end %x\n", i, vma_below_5->vm_start, vma_below_5->vm_end);
        }
        assert(vma_below_5 == NULL);
    }

    mm_destroy(mm);

  //  assert(nr_free_pages_store == nr_free_pages());

    cprintf("check_vma_struct() succeeded!\n");
}

struct mm_struct *check_mm_struct;  // 用来做测试使用

// check_pgfault - check correctness of pgfault handler
static void check_pgfault(void)
{
//    size_t nr_free_pages_store = nr_free_pages();

    check_mm_struct = mm_create();
    assert(check_mm_struct != NULL);

    struct mm_struct *mm = check_mm_struct;
    pde_t *pgdir = mm->pgdir = boot_pgdir;
    assert(pgdir[0] == 0);

    struct vma_struct *vma = vma_create(0, PTSIZE, VM_WRITE);
    assert(vma != NULL);

    insert_vma_struct(mm, vma);

    uintptr_t addr = 0x100;
    assert(find_vma(mm, addr) == vma);

    int i, sum = 0;
    for (i = 0; i < 100; i ++)
    {
        // 这一句在 QEMU 3.1.0 上会触发 page fault 中断，理论上也确实应该如此
        // 因为这个虚拟地址没有做映射，但在新版本 QEMU 4.0.0 上为啥直接运行下去了
        *(char *)(addr + i) = i;
        sum += i;
    }
    for (i = 0; i < 100; i ++)
    {
        sum -= *(char *)(addr + i);
    }
    assert(sum == 0);

    page_remove(pgdir, ROUNDDOWN(addr, PGSIZE));
//    free_page(pde2page(pgdir[0]));
    pgdir[0] = 0;

    mm->pgdir = NULL;
//    mm_destroy(mm);
    check_mm_struct = NULL;

//    assert(nr_free_pages_store == nr_free_pages());

    cprintf("check_pgfault() succeeded!\n");
}

//page fault number
volatile unsigned int pgfault_num = 0;

/* do_pgfault - interrupt handler to process the page fault execption
 * @mm         : the control struct for a set of vma using the same PDT
 * @error_code : the error code recorded in trapframe->tf_err which is setted by x86 hardware
 * @addr       : the addr which causes a memory access exception, (the contents of the CR2 register)
 *
 * CALL GRAPH: trap--> trap_dispatch-->pgfault_handler-->do_pgfault
 * The processor provides ucore's do_pgfault function with two items of information to aid in diagnosing
 * the exception and recovering from it.
 *   (1) The contents of the CR2 register. The processor loads the CR2 register with the
 *       32-bit linear address that generated the exception. The do_pgfault fun can
 *       use this address to locate the corresponding page directory and page-table
 *       entries.
 *   (2) An error code on the kernel stack. The error code for a page fault has a format different from
 *       that for other exceptions. The error code tells the exception handler three things:
 *         -- The P flag   (bit 0) indicates whether the exception was due to a not-present page (0)
 *            or to either an access rights violation or the use of a reserved bit (1).
 *         -- The W/R flag (bit 1) indicates whether the memory access that caused the exception
 *            was a read (0) or write (1).
 *         -- The U/S flag (bit 2) indicates whether the processor was executing at user mode (1)
 *            or supervisor mode (0) at the time of the exception.
 */
/*
 当启动分页机制以后，如果一条指令或数据的虚拟地址所对应的物理页框不在内存中或者访问的类型有错误
 （比如写一个只读页或用户态程序访问内核态的数据等），就会发生页访问异常。
 产生页访问异常的原因主要有：
 目标页帧不存在（页表项全为0，即该线性地址与物理地址尚未建立映射或者已经撤销)；
 相应的物理页帧不在内存中（页表项非空，但 Present 标志位 = 0，比如在 swap 分区或磁盘文件上)，
 不满足访问权限(此时页表项 P 标志 = 1，但低权限的程序试图访问高权限的地址空间，或者有程序试图写只读页面).
 CPU会把产生异常的线性地址存储在 CR2 中，并且把表示页访问异常类型的值（简称页访问异常错误码，errorCode）保存在中断栈中
 页访问异常错误码有 32 位。
 位 0 为 １ 表示对应物理页不存在；
 位 1 为 １ 表示写异常（比如写了只读页)；
 位 2 为 １ 表示访问权限异常（比如用户态程序访问内核空间的数据）
 CR2 是页故障线性地址寄存器，保存最后一次出现页故障的全 32 位线性地址。
 CR2 用于发生页异常时报告出错信息。当发生页异常时，处理器把引起页异常的线性地址保存在 CR2 中。
 操作系统中对应的中断服务例程可以检查 CR2 的内容，从而查出线性地址空间中的哪个页引起本次异常。
 */
int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr)
{
    int ret = -E_INVAL;
    //try to find a vma which include addr
    struct vma_struct *vma = find_vma(mm, addr);

    pgfault_num++;
    //If the addr is in the range of a mm's vma?
    // 缺页中断都是由当前进程代码引起的，先判断下是否访问了非法地址
    if (vma == NULL || vma->vm_start > addr)
    {
        cprintf("not valid addr %x, and  can not find it in vma\n", addr);
        goto failed;
    }
    //check the error_code
    switch (error_code & 3)
    {
        default:
                /* error code flag : default is 3 ( W/R=1, P=1): write, present */
        case 2: /* error code flag : (W/R=1, P=0): write, not present */
            if (!(vma->vm_flags & VM_WRITE))
            {
                cprintf("do_pgfault failed: error code flag = write AND not present, but the addr's vma cannot write\n");
                goto failed;
            }
            break;
            
        case 1: /* error code flag : (W/R=0, P=1): read, present */
            cprintf("do_pgfault failed: error code flag = read AND present\n");
            goto failed;
            
        case 0: /* error code flag : (W/R=0, P=0): read, not present */
            if (!(vma->vm_flags & (VM_READ | VM_EXEC)))
            {
                cprintf("do_pgfault failed: error code flag = read AND not present, but the addr's vma cannot read or exec\n");
                goto failed;
            }
    }
    /* IF (write an existed addr ) OR
     *    (write an non_existed addr && addr is writable) OR
     *    (read  an non_existed addr && addr is readable)
     * THEN
     *    continue process
     */
    uint32_t perm = PTE_U;
    if (vma->vm_flags & VM_WRITE)
    {
        perm |= PTE_W;
    }
    addr = ROUNDDOWN(addr, PGSIZE);

    ret = -E_NO_MEM;

    pte_t *ptep = NULL;
    
    // try to find a pte, if pte's PT(Page Table) isn't existed, then create a PT.
    // (notice the 3th parameter '1')
    // 根据引发缺页异常的地址去找到地址所对应的 PTE，如果找不到则创建一页表
    if ((ptep = get_pte(mm->pgdir, addr, 1)) == NULL)
    {
        cprintf("get_pte in do_pgfault failed\n");
        goto failed;
    }
    
    // 为 0 代表没有与物理页面建立映射关系
    if (*ptep == 0)
    {
        // if the phy addr isn't exist, then alloc a page & map the phy addr with logical addr
        // PTE 所指向的物理页表地址若不存在则分配一物理页并将逻辑地址和物理地址作映射
        // 就是让 PTE 指向 物理页帧
        if (pgdir_alloc_page(mm, addr, perm) == NULL)
        {
            cprintf("pgdir_alloc_page in do_pgfault failed\n");
            goto failed;
        }
    }
    else
    {
        // 表明已经将该虚拟地址对应的物理地址置换在 swap 分区了
        struct Page *page = NULL;
        cprintf("do pgfault: ptep %x, pte %x\n", ptep, *ptep);
        if (*ptep & PTE_P)
        {
            //if process write to this existed readonly page (PTE_P means existed), then should be here now.
            //we can implement the delayed memory space copy for fork child process (AKA copy on write, COW).
            //we didn't implement now, we will do it in future.
            panic("error write a non-writable pte");
            //page = pte2page(*ptep);
        }
        else
        {
            // if this pte is a swap entry, then load data from disk to a page with phy addr
            // and call page_insert to map the phy addr with logical addr
            // 如果 PTE 存在 说明此时 P 位为 0 该页被换出到外存中 需要将其换入内存
            // 该页面存放在外存什么地方呢，地址就存储在 pte 中
            if (swap_init_ok)
            {
                // 根据 PTE 找到 换出那页所在的硬盘地址 并将其从外存中换入
                if ((ret = swap_in(mm, addr, &page)) != 0)
                {
                    cprintf("swap_in in do_pgfault failed\n");
                    goto failed;
                }
            }
            else
            {
                cprintf("no swap_init_ok but ptep is %x, failed\n",*ptep);
                goto failed;
            }
        }
        
        // 建立虚拟地址和物理地址之间的对应关系(更新 PTE 因为 已经被换入到内存中了)
        page_insert(mm->pgdir, page, addr, perm);
        // 维护一个页面替换队列, 使这一页可以置换
        swap_map_swappable(mm, addr, page, 1);
        // 设置这一页的虚拟地址
        page->pra_vaddr = addr;
    }
    ret = 0;
failed:
    return ret;
}

bool user_mem_check(struct mm_struct *mm, uintptr_t addr, size_t len, bool write)
{
    if (mm != NULL)
    {
        if (!USER_ACCESS(addr, addr + len))
        {
            return 0;
        }
        struct vma_struct *vma;
        uintptr_t start = addr, end = addr + len;
        while (start < end)
        {
            if ((vma = find_vma(mm, start)) == NULL || start < vma->vm_start)
            {
                return 0;
            }
            if (!(vma->vm_flags & ((write) ? VM_WRITE : VM_READ)))
            {
                return 0;
            }
            if (write && (vma->vm_flags & VM_STACK))
            {
                if (start < vma->vm_start + PGSIZE)
                {
                    //check stack start & size
                    return 0;
                }
            }
            start = vma->vm_end;
        }
        return 1;
    }
    return KERN_ACCESS(addr, addr + len);
}

bool copy_string(struct mm_struct *mm, char *dst, const char *src, size_t maxn)
{
    size_t alen, part = ROUNDDOWN((uintptr_t)src + PGSIZE, PGSIZE) - (uintptr_t)src;
    while (1)
    {
        if (part > maxn)
        {
            part = maxn;
        }
        if (!user_mem_check(mm, (uintptr_t)src, part, 0))
        {
            return 0;
        }
        if ((alen = strnlen(src, part)) < part)
        {
            memcpy(dst, src, alen + 1);
            return 1;
        }
        if (part == maxn)
        {
            return 0;
        }
        memcpy(dst, src, part);
        dst += part;
        src += part;
        maxn -= part;
        part = PGSIZE;
    }
}

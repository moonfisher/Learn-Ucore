#ifndef __KERN_MM_VMM_H__
#define __KERN_MM_VMM_H__

#include "defs.h"
#include "list.h"
#include "memlayout.h"
#include "sync.h"
#include "proc.h"
#include "sem.h"

//pre define
struct mm_struct;

// the virtual continuous memory area(vma)
/*
 vm_start 和 vm_end 描述了一个连续地址的虚拟内存空间的起始位置和结束位置
 这两个值都应该是 PGSIZE 对齐的，而且描述的是一个合理的地址空间范围（即严格确保 vm_start < vm_end的关系）
 list_link 是一个双向链表，按照从小到大的顺序把一系列用 vma_struct 表示的虚拟内存空间链接起来，
 并且还要求这些链起来的 vma_struct 应该是不相交的，即 vma 之间的地址空间无交集
 vm_flags 表示了这个虚拟内存空间的属性
*/
struct vma_struct
{
    struct mm_struct *vm_mm; // the set of vma using the same PDT 
    uintptr_t vm_start;      // start addr of vma
    uintptr_t vm_end;        // end addr of vma
    uint32_t vm_flags;       // flags of vma
    list_entry_t list_link;  // linear list link which sorted by start addr of vma
    struct shmem_struct *shmem;
    size_t shmem_off;
};

#define le2vma(le, member)                  \
    to_struct((le), struct vma_struct, member)

#define VM_READ                 0x00000001
#define VM_WRITE                0x00000002
#define VM_EXEC                 0x00000004
#define VM_STACK                0x00000008
#define VM_SHARE                0x00000010

// the control struct for a set of vma using the same PDT
/*
 mmap_list 是双向链表头，链接了所有属于同一页目录表的虚拟内存空间
*/
struct mm_struct
{
    list_entry_t mmap_list;        // linear list link which sorted by start addr of vma
    // mmap_cache 是指向当前正在使用的虚拟内存空间，由于操作系统执行的“局部性”原理，
    // 当前正在用到的虚拟内存空间在接下来的操作中可能还会用到，这时就不需要查链表，
    // 而是直接使用此指针就可找到下一次要用到的虚拟内存空间。
    // 由于 mmap_cache 的引入，可使得 mm_struct 数据结构的查询加速 30% 以上。
    struct vma_struct *mmap_cache; // current accessed vma, used for speed purpose
    // pgdir 所指向的就是 mm_struct 数据结构所维护的页表。
    // 通过访问 pgdir 可以查找某虚拟地址对应的页表项是否存在以及页表项的属性等。
    pde_t *pgdir;                  // the PDT of these vma
    // map_count 记录 mmap_list 里面链接的 vma_struct 的个数。
    int map_count;                 // the count of these vma
    // sm_priv 指向用来链接记录页访问情况的链表头，这建立了 mm_struct 和 swap_manager 之间的联系。
    void *sm_priv;                 // the private data for swap manager
    int mm_count;                  // the number ofprocess which shared the mm
    semaphore_t mm_sem;            // mutex for using dup_mmap fun to duplicat the mm 
    int locked_by;                 // the lock owner process's pid
    uintptr_t brk_start, brk;
    list_entry_t proc_mm_link;
};

struct vma_struct *find_vma(struct mm_struct *mm, uintptr_t addr);
struct vma_struct *find_vma_intersection(struct mm_struct *mm, uintptr_t start, uintptr_t end);
struct vma_struct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags);
void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma);

struct mm_struct *mm_create(void);
void mm_destroy(struct mm_struct *mm);

void vmm_init(void);
int mm_map(struct mm_struct *mm, uintptr_t addr, size_t len, uint32_t vm_flags,
           struct vma_struct **vma_store);
int mm_map_shmem(struct mm_struct *mm, uintptr_t addr, uint32_t vm_flags,
        struct shmem_struct *shmem, struct vma_struct **vma_store);
int mm_unmap(struct mm_struct *mm, uintptr_t addr, size_t len);
int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr);

int mm_unmap(struct mm_struct *mm, uintptr_t addr, size_t len);
int dup_mmap(struct mm_struct *to, struct mm_struct *from);
void exit_mmap(struct mm_struct *mm);
uintptr_t get_unmapped_area(struct mm_struct *mm, size_t len);
int mm_brk(struct mm_struct *mm, uintptr_t addr, size_t len);
void dump_vma(struct mm_struct* mm);

extern volatile unsigned int pgfault_num;
extern struct mm_struct *check_mm_struct;

bool user_mem_check(struct mm_struct *mm, uintptr_t start, size_t len, bool write);
bool copy_from_user(struct mm_struct *mm, void *dst, const void *src, size_t len, bool writable);
bool copy_to_user(struct mm_struct *mm, void *dst, const void *src, size_t len);
bool copy_string(struct mm_struct *mm, char *dst, const char *src, size_t maxn);

static inline int mm_count(struct mm_struct *mm)
{
    return mm->mm_count;
}

static inline void set_mm_count(struct mm_struct *mm, int val)
{
    mm->mm_count = val;
}

static inline int mm_count_inc(struct mm_struct *mm)
{
    mm->mm_count += 1;
    return mm->mm_count;
}

static inline int mm_count_dec(struct mm_struct *mm)
{
    mm->mm_count -= 1;
    return mm->mm_count;
}

static inline void lock_mm(struct mm_struct *mm)
{
    if (mm != NULL)
    {
        down(&(mm->mm_sem));
        if (current != NULL)
        {
            mm->locked_by = current->pid;
        }
    }
}

static inline void unlock_mm(struct mm_struct *mm)
{
    if (mm != NULL)
    {
        up(&(mm->mm_sem));
        mm->locked_by = 0;
    }
}

#endif /* !__KERN_MM_VMM_H__ */


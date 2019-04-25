#ifndef __KERN_MM_PMM_H__
#define __KERN_MM_PMM_H__

#include "defs.h"
#include "mmu.h"
#include "memlayout.h"
#include "atomic.h"
#include "assert.h"
#include "vmm.h"

// pmm_manager is a physical memory management class. A special pmm manager - XXX_pmm_manager
// only needs to implement the methods in pmm_manager class, then XXX_pmm_manager can be used
// by ucore to manage the total physical memory space.
// 内存管理器，代码模块化，组件化
struct pmm_manager
{
    const char *name;                                 // XXX_pmm_manager's name
    void (*init)(void);                               // initialize internal description&management data structure
                                                      // (free block list, number of free block) of XXX_pmm_manager 
    void (*init_memmap)(struct Page *base, size_t n); // setup description&management data structcure according to
                                                      // the initial free physical memory space 
    struct Page *(*alloc_pages)(size_t n);            // allocate >=n pages, depend on the allocation algorithm 
    void (*free_pages)(struct Page *base, size_t n);  // free >=n pages with "base" addr of Page descriptor structures(memlayout.h)
    size_t (*nr_free_pages)(void);                    // return the number of free pages 
    void (*check)(void);                              // check the correctness of XXX_pmm_manager 
};

extern const struct pmm_manager *pmm_manager;
extern pde_t *boot_pgdir;
extern uintptr_t boot_cr3;

void pmm_init(void);

struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);

#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)

pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create);
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store);
void page_remove(pde_t *pgdir, uintptr_t la);
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);

void load_esp0(uintptr_t esp0);
void tlb_invalidate(pde_t *pgdir, uintptr_t la);
struct Page *pgdir_alloc_page(struct mm_struct *mm, uintptr_t la, uint32_t perm);
void unmap_range(pde_t *pgdir, uintptr_t start, uintptr_t end);
void exit_range(pde_t *pgdir, uintptr_t start, uintptr_t end);
int copy_range(struct mm_struct *to, struct mm_struct *from, uintptr_t start, uintptr_t end, bool share);

void print_pgdir(void);

/* *
 * PADDR - takes a kernel virtual address (an address that points above KERNBASE),
 * where the machine's maximum 256MB of physical memory is mapped and returns the
 * corresponding physical address.  It panics if you pass it a non-kernel virtual address.
 * */
// 虚拟地址转物理地址，增加有效性判断
#define PADDR(kva) ({                                                   \
            uintptr_t __m_kva = (uintptr_t)(kva);                       \
            if (__m_kva < KERNBASE) {                                   \
                panic("PADDR called with invalid kva %08lx", __m_kva);  \
            }                                                           \
            __m_kva - KERNBASE;                                         \
        })

/* *
 * KADDR - takes a physical address and returns the corresponding kernel virtual
 * address. It panics if you pass an invalid physical address.
 * */
// 物理地址转虚拟地址，增加有效性判断
#define KADDR(pa) ({                                                    \
            uintptr_t __m_pa = (pa);                                    \
            size_t __m_ppn = PPN(__m_pa);                               \
            if (__m_ppn >= npage) {                                     \
                panic("KADDR called with invalid pa %08lx", __m_pa);    \
            }                                                           \
            (void *) (__m_pa + KERNBASE);                               \
        })

extern struct Page *pages;
extern size_t npage;

// 根据 page 获取页表索引
static inline ppn_t page2ppn(struct Page *page)
{
    ppn_t ppn = (ppn_t)(page - pages);
    return ppn;
}

// 根据 page 获取页表物理地址
static inline uintptr_t page2pa(struct Page *page)
{
    uintptr_t pa = page2ppn(page) << PGSHIFT;
    return pa;
}

// 根据物理地址获取所在 page 页
static inline struct Page *pa2page(uintptr_t pa)
{
    uintptr_t ppn = PPN(pa);
    if (ppn >= npage)
    {
        panic("pa2page called with invalid pa");
    }
    return &pages[ppn];
}

// 根据 page 获取页表虚拟地址
static inline void *page2kva(struct Page *page)
{
    void *kva = KADDR(page2pa(page));
    return kva;
}

// 根据虚拟地址获取所在 page 页
static inline struct Page *kva2page(void *kva)
{
    struct Page *page = pa2page(PADDR(kva));
    return page;
}

// 根据页表物理地址获取所在 page 页
static inline struct Page *pte2page(pte_t pte)
{
    struct Page *page = pa2page(PTE_ADDR(pte));
    if (!(pte & PTE_P))
    {
        panic("pte2page called with invalid pte");
    }
    return page;
}

// 根据页目录物理地址所在 page 页
static inline struct Page *pde2page(pde_t pde)
{
    struct Page *page = pa2page(PDE_ADDR(pde));
    return page;
}

static inline int page_ref(struct Page *page)
{
    return page->ref;
}

static inline void set_page_ref(struct Page *page, int val)
{
    page->ref = val;
}

static inline int page_ref_inc(struct Page *page)
{
    page->ref += 1;
    return page->ref;
}

static inline int page_ref_dec(struct Page *page)
{
    page->ref -= 1;
    return page->ref;
}

#endif /* !__KERN_MM_PMM_H__ */


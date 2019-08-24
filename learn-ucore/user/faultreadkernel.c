#include "stdio.h"
#include "ulib.h"

int main(void)
{
    /*
     只有当一级二级页表的项都设置了用户写权限后，用户才能对对应的物理地址进行读写。
     可以在一级页表先给用户写权限，再在二级页表上面根据需要限制用户的权限，对物理页进行保护。
     由于一个物理页可能被映射到不同的虚拟地址上去（譬如一块内存在不同进程间共享）
     
     -------------------- BEGIN --------------------
     PDE(001) 00000000-00400000 00400000 urw
     |-- PTE(00007) 00200000-00207000 00007000 urw
     
     这段是用户代码地址空间，有 PTE_U 用户访问权限，写权限根据 ELF_PF_W 来控制
     PDE(001) 00800000-00c00000 00400000 urw
     |-- PTE(00002) 00800000-00802000 00002000 ur-
     |-- PTE(00001) 00802000-00803000 00001000 urw
     
     这段是用户堆栈空间，有 PTE_U 用户读写权限
     PDE(001) afc00000-b0000000 00400000 urw
     |-- PTE(00004) afffc000-b0000000 00004000 urw
     
     这段是内核地址空间，没有 PTE_U 用户访问权限
     PDE(0e0) c0000000-f8000000 38000000 -rw
     |-- PTE(38000) c0000000-f8000000 38000000 -rw
     
     这段是物理页表所对应的虚拟地址空间，没有设置 PTE_U 用户访问权限就无法访问
     PDE(001) fac00000-fb000000 00400000 -rw
     |-- PTE(00001) fac00000-fac01000 00001000 urw
     |-- PTE(00001) fac02000-fac03000 00001000 urw
     |-- PTE(00001) faebf000-faec0000 00001000 urw
     |-- PTE(000e0) faf00000-fafe0000 000e0000 -rw
     |-- PTE(00001) fafeb000-fafec000 00001000 -rw
     --------------------- END ---------------------
    */
    
    /*
     通过打印页表就可以看出哪些页面可以访问，哪些无法访问。
     1）地址在当前进程页表里找不到映射，比如访问 NULL(0)，会产生 T_PGFLT 中断
     2）地址在当前进程页表里有映射，但没有 PTE_U 用户访问权限，比如访问内核地址空间，
        也会产生 T_PGFLT 中断
     
     T_PGFLT 中断之后，操作系统检查进程 mm 表，发现当前地址也不在该进程可访问的空间内，
     直接 kill 进程，
    */
    print_pgdir();
    print_vm();
    
    // 可以访问
    cprintf("I read %08x from 0x00200000!\n", *(unsigned *)0x00200000);
    cprintf("I read %08x from 0x00800000!\n", *(unsigned *)0x00800000);
    cprintf("I read %08x from 0xafffc000!\n", *(unsigned *)0xafffc000);
    
    // 无法访问，T_PGFLT 中断
    cprintf("I read %08x from 0x0!\n", *(unsigned *)NULL);
    cprintf("I read %08x from 0xc0000000!\n", *(unsigned *)0xc0000000);
    cprintf("I read %08x from 0xfac00000!\n", *(unsigned *)0xfac00000);
}


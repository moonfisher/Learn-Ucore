/* See COPYRIGHT for copyright information. */

#include "mmu.h"
#include "memlayout.h"

###################################################################
# entry point for APs
###################################################################

# Each non-boot CPU ("AP") is started up in response to a STARTUP
# IPI from the boot CPU.  Section B.4.2 of the Multi-Processor
# Specification says that the AP will start in real mode with CS:IP
# set to XY00:0000, where XY is an 8-bit value sent with the
# STARTUP. Thus this code must start at a 4096-byte boundary.
#
# Because this code sets DS to zero, it must run from an address in
# the low 2^16 bytes of physical memory.
#
# boot_aps() (in init.c) copies this code to MPENTRY_PADDR (which
# satisfies the above restrictions).  Then, for each AP, it stores the
# address of the pre-allocated per-core stack in mpentry_kstack, sends
# the STARTUP IPI, and waits for this code to acknowledge that it has
# started (which happens in mp_main in init.c).
#
# This code is similar to boot/boot.S except that
#    - it does not need to enable A20
#    - it uses MPBOOTPHYS to calculate absolute addresses of its
#      symbols, rather than relying on the linker to fill them

#define REALLOC(x) (x - KERNBASE)
#define MPBOOTPHYS(s) ((s) - mpentry_start + MPENTRY_PADDR)

.set PROT_MODE_CSEG, 0x8	# kernel code segment selector
.set PROT_MODE_DSEG, 0x10	# kernel data segment selector

.code16           
.globl mpentry_start
mpentry_start:
	cli            

	xorw    %ax, %ax
	movw    %ax, %ds
	movw    %ax, %es
	movw    %ax, %ss

	lgdt    MPBOOTPHYS(gdtdesc)
	movl    %cr0, %eax
	orl     $CR0_PE, %eax
	movl    %eax, %cr0

	ljmpl   $(PROT_MODE_CSEG), $(MPBOOTPHYS(start32))

.code32
start32:
	movw    $(PROT_MODE_DSEG), %ax
	movw    %ax, %ds
	movw    %ax, %es
	movw    %ax, %ss
	movw    $0, %ax
	movw    %ax, %fs
	movw    %ax, %gs

	# Set up initial page table. We cannot use kern_pgdir yet because
	# we are still running at a low EIP.
    # AP 初始化也是从实模式开始的，也需要加载临时分页，和 BSP 初始化类似
	movl    $(REALLOC(__boot_pgdir_ap)), %eax
	movl    %eax, %cr3
	# Turn on paging.
	movl    %cr0, %eax
	orl     $(CR0_PE | CR0_PG | CR0_WP), %eax
	movl    %eax, %cr0

	# Switch to the per-cpu stack allocated in boot_aps()
	movl    mpentry_kstack, %esp
	movl    $0x0, %ebp       # nuke frame pointer

	# Call mp_main().  (Exercise for the reader: why the indirect call?)
	movl    $mp_main, %eax
	call    *%eax

	# If mp_main returns (it shouldn't), loop.
spin:
	jmp     spin

# kernel builtin pgdir
# an initial page directory (Page Directory Table, PDT)
# These page directory table and page table can be reused!
.align PGSIZE
__boot_pgdir_ap:
.globl __boot_pgdir_ap
    # map va 0 ~ 4M to pa 0 ~ 4M (temporary)
    .long REALLOC(__boot_pt_ap) + (PTE_P | PTE_U | PTE_W)
    .space (KERNBASE >> PGSHIFT >> 10 << 2) - (. - __boot_pgdir_ap) # pad to PDE of KERNBASE
    # map va KERNBASE + (0 ~ 4M) to pa 0 ~ 4M
    .long REALLOC(__boot_pt_ap) + (PTE_P | PTE_U | PTE_W)
    .space PGSIZE - (. - __boot_pgdir_ap) # pad to PGSIZE

.set i, 0
__boot_pt_ap:
    .rept 1024
    .long i * PGSIZE + (PTE_P | PTE_W)
    .set i, i + 1
.endr

# Bootstrap GDT
.p2align 2					# force 4 byte alignment
gdt:
    SEG_NULL                                        # null seg
    SEG_ASM(STA_X | STA_R , 0x0, 0xffffffff)        # code seg for bootloader and kernel
    SEG_ASM(STA_W , 0x0, 0xffffffff)                # data seg for bootloader and kernel

gdtdesc:
    .word 0x17                                      # sizeof(gdt) - 1
    .long gdt                                       # address gdt 0x7DAC

.globl mpentry_end
mpentry_end:
	nop


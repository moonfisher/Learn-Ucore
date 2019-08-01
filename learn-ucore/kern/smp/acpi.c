/* vm64.c 
 *
 * Copyright (c) 2013 Brian Swetland
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "defs.h"
#include "cpu.h"
#include "memlayout.h"
#include "x86.h"
#include "mmu.h"
#include "proc.h"
#include "acpi.h"
#include "stdio.h"
#include "pmm.h"
#include "string.h"

#define PHYSLIMIT       KMEMSIZE

static struct acpi_rdsp *scan_rdsp(uint32_t base, uint32_t len)
{
    uint8_t *p;
    for (p = KADDR(base); len >= sizeof(struct acpi_rdsp); len -= 4, p += 4)
    {
        if (memcmp(p, SIG_RDSP, 8) == 0)
        {
            uint32_t sum, n;
            for (sum = 0, n = 0; n < 20; n++)
                sum += p[n];
            if ((sum & 0xff) == 0)
                return (struct acpi_rdsp *) p;
        }
    }
    
    return (struct acpi_rdsp *) 0;
}

static struct acpi_rdsp *find_rdsp(void)
{
    struct acpi_rdsp *rdsp;
    uint32_t pa;
    
    pa = *((uint16_t *)KADDR(0x40E)) << 4; // EBDA
    if (pa && (rdsp = scan_rdsp(pa, 1024)))
        return rdsp;
    
    return scan_rdsp(0xE0000, 0x20000);
} 

/*
 (gdb) p *madt
 {
    header = {
        signature = "APIC",
        length = 240,
        revision = 1 '\001',
        checksum = 254 '\376',
        oem_id = "BOCHS ",
        oem_tableid = "BXPCAPIC",
        oem_revision = 1,
        creator_id = "BXPC",
        creator_revision = 1
    },
    lapic_addr_phys = 0xfee00000,
    flags = 1,
    table = 0xdffe1772
 }
*/
static int acpi_config_smp(struct acpi_madt *madt)
{
    uint32_t lapic_addr;
    uint32_t nioapic = 0;
    uint8_t *p, *e;

    if (!madt)
        return -1;
    
    if (madt->header.length < sizeof(struct acpi_madt))
        return -1;

    lapic_addr = madt->lapic_addr_phys;

    p = madt->table;
    e = p + madt->header.length - sizeof(struct acpi_madt);

    while (p < e)
    {
        uint32_t len;
        
        if ((e - p) < 2)
            break;
        len = p[1];
        if ((e - p) < len)
            break;
        
        switch (p[0])
        {
            case TYPE_LAPIC:
            {
                struct madt_lapic *lapic = (void*) p;
                if (len < sizeof(*lapic))
                    break;
                if (!(lapic->flags & APIC_LAPIC_ENABLED))
                    break;
                
                // acpi 这里统计的是虚拟 cpu 线程数，等于 cpu 物理个数 * 核心数 * 线程数
                // 这里和 bios 记录的 mp 信息不同，mp 记录的是物理 cpu 个数
                cprintf("acpi: cpu %d, type %d, length %d, apicid %d, acpiid %d, flags %x\n", ncpu, lapic->type, lapic->length, lapic->apic_id, lapic->acpi_id, lapic->flags);
                cpus[ncpu].cpu_id = ncpu;
                cpus[ncpu].apic_id = lapic->apic_id;
                cpus[ncpu].acpi_id = lapic->acpi_id;
                ncpu++;
                break;
            }
            case TYPE_IOAPIC:
            {
                struct madt_ioapic *ioapic = (void*) p;
                if (len < sizeof(*ioapic))
                    break;
                
                cprintf("acpi: ioapic %d, type %d, length %d, addr %x, id = %d, base = %x\n", nioapic, ioapic->type, ioapic->length, ioapic->addr, ioapic->id, ioapic->interrupt_base);
                if (nioapic)
                {
                    cprintf("warning: multiple ioapics are not supported");
                }
                else
                {
                    ioapicid = ioapic->id;
                    ioapicaddr = ioapic->addr;
                }
                nioapic++;
                break;
            }
            case TYPE_INT_SRC_OVERRIDE:
            {
                cprintf("acpi: int src override\n");
                break;
            }
            case TYPE_NMI_INT_SRC:
            {
                cprintf("acpi: nmi int src\n");
                break;
            }
            case TYPE_LAPIC_NMI:
            {
                cprintf("acpi: lapic nmi\n");
                break;
            }
        }
        p += len;
    }

    if (ncpu)
    {
        ismp = 1;
        lapicaddr = lapic_addr;
        // lapicaddr 这个是一个 MMIO 地址，也是物理地址，但不在内存上
        // 这里需要映射到虚拟地址才能访问
        lapic = mmio_map_region(lapicaddr, 4096);
        cprintf("acpi_config_smp mmio_map_region: lapicaddr:%x, lapic:%x\n", lapicaddr, lapic);
        return 0;
    }

    return -1;
}

/*
 qemu smp 配置参考 https://zhensheng.im/2015/11/22/2521/MIAO_LE_GE_MI
 acpi 信息如果有的话，是放在物理内存最后
 
 如果物理内存是 512m：
 (gdb) p * rdsp
 {
    signature = "RSD PTR ",
    checksum = 175 '\257',
    oem_id = "BOCHS ",
    revision = 0 '\000',
    rsdt_addr_phys = 0x1ffe186e,
    length = 0,
    xsdt_addr_phys = 0,
    xchecksum = 95 '_',
    reserved = "SM_"
 }
 
 (gdb) p *rsdt
 {
    header = {
        signature = "RSDT",
        length = 48,
        revision = 1 '\001',
        checksum = 128 '\200',
        oem_id = "BOCHS ",
        oem_tableid = "BXPCRSDT",
        oem_revision = 1,
        creator_id = "BXPC",
        creator_revision = 1
    },
    entry = 0xdffe1892
 }
 
 (gdb) p *hdr
 {
    signature = "FACP",
    length = 116,
    revision = 1 '\001',
    checksum = 43 '+',
    oem_id = "BOCHS ",
    oem_tableid = "BXPCFACP",
    oem_revision = 1,
    creator_id = "BXPC",
    creator_revision = 1
 }
 
 {
    signature = "APIC",
    length = 240,
    revision = 1 '\001',
    checksum = 254 '\376',
    oem_id = "BOCHS ",
    oem_tableid = "BXPCAPIC",
    oem_revision = 1,
    creator_id = "BXPC",
    creator_revision = 1
 }
 
 {
    signature = "HPET",
    length = 56,
    revision = 1 '\001',
    checksum = 3 '\003',
    oem_id = "BOCHS ",
    oem_tableid = "BXPCHPET",
    oem_revision = 1,
    creator_id = "BXPC",
    creator_revision = 1
 }
*/
int acpi_init(void)
{
    unsigned n, count;
    struct acpi_rdsp *rdsp;
    struct acpi_rsdt *rsdt;
    struct acpi_madt *madt = 0;

    rdsp = find_rdsp();
    if (rdsp->rsdt_addr_phys > PHYSLIMIT)
        goto notmapped;
    
    rsdt = (struct acpi_rsdt *)(rdsp->rsdt_addr_phys + KERNBASE);
    count = (rsdt->header.length - sizeof(*rsdt)) / 4;
    for (n = 0; n < count; n++)
    {
        if (rsdt->entry[n] > PHYSLIMIT)
            goto notmapped;
        
        struct acpi_desc_header *hdr = (struct acpi_desc_header *)(rsdt->entry[n] + KERNBASE);
        if (!memcmp(hdr->signature, SIG_MADT, 4))
            madt = (void*) hdr;
    }

    return acpi_config_smp(madt);

notmapped:
    cprintf("acpi: tables above 0x%x not mapped.\n", PHYSLIMIT);
    return -1;
}



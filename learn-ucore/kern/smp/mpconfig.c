// Search for and parse the multiprocessor configuration table
// See http://developer.intel.com/design/pentium/datashts/24201606.pdf

#include "defs.h"
#include "string.h"
#include "memlayout.h"
#include "x86.h"
#include "mmu.h"
//#include "env.h"
#include "cpu.h"
#include "pmm.h"
#include "stdio.h"

struct cpu_info cpus[NCPU];
struct cpu_info *bootcpu;
int ismp;
int ncpu; //总 CPU 数，不超过 NCPU(8)

// Per-CPU kernel stacks
unsigned char percpu_kstacks[NCPU][KSTACKSIZE] __attribute__((aligned(PGSIZE)));

// See MultiProcessor Specification Version 1.[14]

struct mp
{
    // floating pointer [MP 4.1]
    uint8_t         signature[4];   // "_MP_"
    physaddr_t      physaddr;       // phys addr of MP config table
    uint8_t         length;         // 1
    uint8_t         specrev;        // [14]
    uint8_t         checksum;       // all bytes must add up to 0
    uint8_t         type;           // MP system config type
    uint8_t         imcrp;
    uint8_t         reserved[3];
} __attribute__((__packed__));

struct mpconf
{
    // configuration table header [MP 4.2]
    uint8_t         signature[4];   // "PCMP"
    uint16_t        length;         // total table length
    uint8_t         version;        // [14]
    uint8_t         checksum;       // all bytes must add up to 0
    uint8_t         product[20];    // product id
    physaddr_t      oemtable;       // OEM table pointer
    uint16_t        oemlength;      // OEM table length
    uint16_t        entry;          // entry count
    physaddr_t      lapicaddr;      // address of local APIC
    uint16_t        xlength;        // extended table length
    uint8_t         xchecksum;      // extended table checksum
    uint8_t         reserved;
    uint8_t         entries[0];     // table entries
} __attribute__((__packed__));

struct mpproc
{
    // processor table entry [MP 4.3.1]
    uint8_t         type;           // entry type (0)
    uint8_t         apicid;         // local APIC id
    uint8_t         version;        // local APIC version
    uint8_t         flags;          // CPU flags
    uint8_t         signature[4];   // CPU signature
    uint32_t        feature;        // feature flags from CPUID instruction
    uint8_t         reserved[8];
} __attribute__((__packed__));

struct mpioapic
{
    // I/O APIC table entry
    uint8_t         type;           // entry type (2)
    uint8_t         apicno;         // I/O APIC id
    uint8_t         version;        // I/O APIC version
    uint8_t         flags;          // I/O APIC flags
    uint32_t        addr;           // I/O APIC address
};

// mpproc flags
#define MPPROC_BOOT     0x02    // This mpproc is the bootstrap processor

// Table entry types
#define MPPROC          0x00  //入口类型为处理器
#define MPBUS           0x01  //入口类型为总线
#define MPIOAPIC        0x02  //入口类型为 I/O APIC
#define MPIOINTR        0x03  //入口类型为 I/O 中断分配
#define MPLINTR         0x04  //入口类型为逻辑中断分配

static uint8_t sum(void *addr, int len)
{
    int i, sum;
    
    sum = 0;
    for (i = 0; i < len; i++)
    {
        sum += ((uint8_t *)addr)[i];
    }
    
    return sum;
}

// Look for an MP structure in the len bytes at physical address addr.
// 此方法将 “MP” 字符串作为了 MP 浮点结构的标识，匹配到此字符串即找到了 MP 浮点结构，
// 本函数返回指向该 MP 浮点结构的指针。
static struct mp *mpsearch1(physaddr_t a, int len)
{
    struct mp *mp = KADDR(a), *end = KADDR(a + len);
    
    for (; mp < end; mp++)
    {
        if (memcmp(mp->signature, "_MP_", 4) == 0 && sum(mp, sizeof(*mp)) == 0)
        {
            return mp;
        }
    }
    return NULL;
}

// Search for the MP Floating Pointer Structure, which according to
// [MP 4] is in one of the following three locations:
// 1) in the first KB of the EBDA;
// 2) if there is no EBDA, in the last KB of system base memory;
// 3) in the BIOS ROM between 0xE0000 and 0xFFFFF.
/*
 当计算机通电时，BIOS 数据区（BIOS Data Area）将在 000400h 处创建。
 它长度为 256 字节（000400h - 0004FFh），包含有关系统环境的信息。该信息可以被任何程序访问和更改。
 计算机的大部分操作由此数据控制。此数据在启动过程中由POST（BIOS开机自检）加载。
 
 如果 EBDA（扩展 BIOS 数据区）不存在，BDA[0x0E] 和 BDA[0x0F] 的值为 0,
 如果 EBDA 存在，其段地址被保存在 BDA[0x0E] 和 BDA[0x0F] 中，其中 BDA[0x0E]
 保存 EBDA 段地址的低 8 位，BDA[0x0F] 保存 EDBA 段地址的高 8 位，所以 (BDA[0x0F]<<8) | BDA[0x0E]
 就表示了 EDBA 的段地址，将段地址左移 4 位即为 EBDA 的物理地址.
 BDA[0x13] 和 BDA[0x14] 分别存放着系统基本内存的大小的低 8 位和高 8 位.
*/
static struct mp *mpsearch(void)
{
    uint8_t *bda;
    uint32_t p;
    struct mp *mp;
    
    static_assert(sizeof(*mp) == 16);
    
    // The BIOS data area lives in 16-bit segment 0x40.
    bda = (uint8_t *)KADDR(0x40 << 4);
    
    // [MP 4] The 16-bit segment of the EBDA is in the two bytes
    // starting at byte 0x0E of the BDA.  0 if not present.
    if ((p = *(uint16_t *)(bda + 0x0E)))
    {
        p <<= 4; // Translate from segment to PA
        if ((mp = mpsearch1(p, 1024)))
            return mp;
    }
    else
    {
        // The size of base memory, in KB is in the two bytes
        // starting at 0x13 of the BDA.
        p = *(uint16_t *)(bda + 0x13) * 1024;
        if ((mp = mpsearch1(p - 1024, 1024)))
            return mp;
    }
    
    return mpsearch1(0xF0000, 0x10000);
}

// Search for an MP configuration table.  For now, don't accept the
// default configurations (physaddr == 0).
// Check for the correct signature, checksum, and version.
static struct mpconf *mpconfig(struct mp **pmp)
{
    struct mpconf *conf;
    struct mp *mp;
    
    if ((mp = mpsearch()) == 0)
        return NULL;
    
    if (mp->physaddr == 0 || mp->type != 0)
    {
        cprintf("SMP: Default configurations not implemented\n");
        return NULL;
    }
    
    conf = (struct mpconf *)KADDR(mp->physaddr);
    if (memcmp(conf, "PCMP", 4) != 0)
    {
        cprintf("SMP: Incorrect MP configuration table signature\n");
        return NULL;
    }
    
    if (sum(conf, conf->length) != 0)
    {
        cprintf("SMP: Bad MP configuration checksum\n");
        return NULL;
    }
    
    if (conf->version != 1 && conf->version != 4)
    {
        cprintf("SMP: Unsupported MP version %d\n", conf->version);
        return NULL;
    }
    
    if ((sum((uint8_t *)conf + conf->length, conf->xlength) + conf->xchecksum) & 0xff)
    {
        cprintf("SMP: Bad MP configuration extended checksum\n");
        return NULL;
    }
    
    *pmp = mp;
    return conf;
}

/*
 smp 原理参考 https://www.jianshu.com/p/fc9a8572a830
 qemu smp 配置参考 https://zhensheng.im/2015/11/22/2521/MIAO_LE_GE_MI
 
 SMP: CPU proc, type:0, apicid:0, version:20, flags:3, signature:c, feature:1781abfd
 SMP: CPU proc, type:0, apicid:4, version:20, flags:1, signature:c, feature:1781abfd
 SMP: CPU proc, type:0, apicid:8, version:20, flags:1, signature:c, feature:1781abfd
 SMP: CPU proc, type:0, apicid:12, version:20, flags:1, signature:c, feature:1781abfd
 SMP: CPU 0 found 4 CPU(s), lapicaddr fee00000
 
 APIC ID 分为 LAPIC ID 和 IOAPIC ID。前者唯一的标识系统中某个 LAPIC，
 后者唯一标识某个 IOAPIC。
 根据 MP spec（Multiple Processor Specification，多处理器规范）规定，LAPIC ID 必须唯一，
 但可以不连续。与 LAPIC ID 不同，IOAPIC ID 在系统 RESET 后统一清零，操作系统或 BIOS 负责验证
 IOAPIC ID 是否唯一，如果有冲突检测到，由操作系统或 BIOS 重新分配。重分配的原则是从系统中所有
 LAPIC ID 后最小的数字开始分配。如当前系统有两个 LAPIC，ID 为 0、1，则分配 IOAPIC ID 应该从 2 开始。
 需要注意的是，MP spec 对 APIC ID 的分配规则只适用于系统用 APIC BUS 的情况。X86 spec 说 4bit
 的 LAPIC ID 可以表示 15 个 CPU，还有一个 0x0f 预留给了广播。当 RTE 的 destination field 字段
 代表 APIC ID，且值为 0x0f 时，该中断消息广播给所有 CPU。
 
 (gdb) p *mp
 {
    signature = "_MP_",
    physaddr = 0xf5ad0,
    length = 1 '\001',
    specrev = 4 '\004',
    checksum = 103 'g',
    type = 0 '\000',
    imcrp = 0 '\000',
    reserved = "\000\000"
 }
 
 (gdb) p *conf
 {
    signature = "PCMP",
    length = 268,
    version = 4 '\004',
    checksum = 87 'W',
    product = "BOCHSCPU0.1",
    oemtable = 0,
    oemlength = 0,
    entry = 22,
    lapicaddr = 0xfee00000,
    xlength = 0,
    xchecksum = 0 '\000',
    reserved = 0 '\000',
    entries = 0xc00f5afc
 }
*/
void mp_init(void)
{
    struct mp *mp;
    struct mpconf *conf;
    struct mpproc *proc;
    struct mpioapic *ioapic;
    uint8_t *p;
    unsigned int i;
    
    bootcpu = &cpus[0];
    if ((conf = mpconfig(&mp)) == 0)
        return;
    
    ismp = 1;
    lapicaddr = conf->lapicaddr;    // 0xFEE00000
    
    for (p = conf->entries, i = 0; i < conf->entry; i++)
    {
        switch (*p)
        {
            case MPPROC:
                proc = (struct mpproc *)p;
                if (proc->flags & MPPROC_BOOT) //如果设置了该标志位表明当前 CPU 是 BSP
                    bootcpu = &cpus[ncpu];
                
                // bios 这里统计的是物理 cpu 的个数，和 acpi 不同
                if (ncpu < NCPU)
                {
                    cprintf("SMP: CPU proc, type:%d, apicid:%d, version:%d, flags:%x, signature:%c%c%c%c, feature:%x\n", proc->type, proc->apicid, proc->version, proc->flags, proc->signature[0], proc->signature[1], proc->signature[2], proc->signature[3], proc->feature);
                    cpus[ncpu].cpu_id = ncpu;
                    cpus[ncpu].apic_id = proc->apicid;
                    ncpu++;
                }
                else
                {
                    cprintf("SMP: too many CPUs, CPU %d disabled\n", proc->apicid);
                }
                
                p += sizeof(struct mpproc);
                continue;
            
            case MPIOAPIC:
                ioapic = (struct mpioapic *)p;
                ioapicid = ioapic->apicno;  // 0x0
                ioapicaddr = ioapic->addr;  // 0xFEC00000
                cprintf("SMP: IOAPIC proc, type:%d, apicno:%x, version:%d, flags:%x, addr:%x\n", ioapic->type, ioapic->apicno, ioapic->version, ioapic->flags, ioapic->addr);
                p += sizeof(struct mpioapic);
                continue;
                
            case MPBUS:
                cprintf("SMP: BUS proc\n");
                p += 8;
                continue;

            case MPIOINTR:
                cprintf("SMP: IOINTR proc\n");
                p += 8;
                continue;
                
            case MPLINTR:
                cprintf("SMP: LINTR proc\n");
                p += 8;
                continue;
                
            default:
                cprintf("SMP: unknown config type %x\n", *p);
                ismp = 0;
                i = conf->entry;
        }
    }
    
    bootcpu->cpu_status = CPU_STARTED;
    if (!ismp)
    {
        // Didn't like what we found; fall back to no MP.
        ncpu = 1;
        lapicaddr = 0;
        cprintf("SMP: configuration not found, SMP disabled\n");
        return;
    }
        
    // lapicaddr is the physical address of the LAPIC's 4K MMIO
    // region.  Map it in to virtual memory so we can access it.
    // lapicaddr 这个是一个 MMIO 地址，也是物理地址，但不在内存上
    // 这里需要映射到虚拟地址才能访问
    lapic = mmio_map_region(lapicaddr, 4096);
    cprintf("SMP mmio_map_region: lapicaddr:%x, lapic:%x\n", lapicaddr, lapic);

    cprintf("SMP: CPU %d found %d CPU(s), lapicaddr %x\n", bootcpu->cpu_id, ncpu, lapicaddr);
    
    if (mp->imcrp)
    {
        // [MP 3.2.6.1] If the hardware implements PIC mode,
        // switch to getting interrupts from the LAPIC.
        cprintf("SMP: Setting IMCR to switch from PIC mode to symmetric I/O mode\n");
        outb(0x22, 0x70);           // Select IMCR
        outb(0x23, inb(0x23) | 1); // Mask external interrupts.
    }
}

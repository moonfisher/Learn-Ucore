// The local APIC manages internal (non-I/O) interrupts.
// See Chapter 8 & Appendix C of Intel processor manual volume 3.

#include "defs.h"
#include "memlayout.h"
#include "trap.h"
#include "mmu.h"
#include "stdio.h"
#include "x86.h"
#include "pmm.h"
#include "cpu.h"
#include "string.h"

// Local APIC registers, divided by 4 for use as uint32_t[] indices.
#define ID          (0x0020 / 4)    // ID
#define VER         (0x0030 / 4)    // Version
#define TPR         (0x0080 / 4)    // Task Priority
#define EOI         (0x00B0 / 4)    // EOI
#define SVR         (0x00F0 / 4)    // Spurious Interrupt Vector
#define ENABLE      0x00000100      // Unit Enable
#define ESR         (0x0280 / 4)    // Error Status
#define ICRLO       (0x0300 / 4)    // Interrupt Command
#define INIT        0x00000500	    // INIT/RESET
#define STARTUP     0x00000600      // Startup IPI
#define DELIVS      0x00001000      // Delivery status
#define ASSERT      0x00004000      // Assert interrupt (vs deassert)
#define DEASSERT    0x00000000
#define LEVEL       0x00008000      // Level triggered
#define BCAST       0x00080000      // Send to all APICs, including self.
#define OTHERS      0x000C0000      // Send to all APICs, excluding self.
#define BUSY        0x00001000
#define FIXED       0x00000000
#define ICRHI       (0x0310 / 4)    // Interrupt Command [63:32]
#define TIMER       (0x0320 / 4)    // Local Vector Table 0 (TIMER)
#define X1          0x0000000B		// divide counts by 1
#define PERIODIC    0x00020000      // Periodic
#define PCINT       (0x0340 / 4)    // Performance Counter LVT
#define LINT0       (0x0350 / 4)    // Local Vector Table 1 (LINT0)
#define LINT1       (0x0360 / 4)    // Local Vector Table 2 (LINT1)
#define ERROR       (0x0370 / 4)    // Local Vector Table 3 (ERROR)
#define MASKED      0x00010000      // Interrupt masked
#define TICR        (0x0380 / 4)    // Timer Initial Count
#define TCCR        (0x0390 / 4)    // Timer Current Count
#define TDCR        (0x03E0 / 4)    // Timer Divide Configuration

#define IO_RTC      0x70

// lapic 是通过 MMIO 映射之后的地址，指向的是每个 CPU 自己的 local apic I/O 地址空间
// 并不是指向内存，看起来多个 CPU 都在访问同一个地址，实际访问的是不同的 I/O 硬件
physaddr_t lapicaddr; // Initialized in mpconfig.c 0xFEE00000

/*
 volatile 关键字是一种类型修饰符，表明某个变量的值可能在外部被改变，因此对这些变量的存取
 不能缓存到寄存器，每次使用时需要重新存取。
 该关键字在多线程环境下经常使用，因为在编写多线程的程序时，同一个变量可能被多个线程修改，
 而程序通过该变量同步各个线程
*/
volatile uint32_t *lapic;

static void lapicw(int index, int value)
{
//    cprintf("lapicw lapic:%x, index:%x, value:%x\n", lapic, index, value);
    lapic[index] = value;
    lapic[index]; // wait for write to finish, by reading
}

void lapic_init(void)
{
    if (!lapicaddr)
        return;

    // lapicaddr is the physical address of the LAPIC's 4K MMIO
    // region.  Map it in to virtual memory so we can access it.
    // 不同 cpu 映射到的虚拟地址 lapic 是不同的
    lapic = mmio_map_region(lapicaddr, 4096);
    cprintf("lapic_init mmio_map_region: lapicaddr:%x, lapic:%x\n", lapicaddr, lapic);

    // Enable local APIC; set spurious interrupt vector.
    // 使用 SVR 来打开 APIC 还可以使用 global enable / disable APIC
    lapicw(SVR, ENABLE | (IRQ_OFFSET + IRQ_SPURIOUS));

    // The timer repeatedly counts down at bus frequency
    // from lapic[TICR] and then issues an interrupt.
    // If we cared more about precise timekeeping,
    // TICR would be calibrated using an external time source.
    // 设置时钟频率
    lapicw(TDCR, X1);
    // 设置定期计数模式
    lapicw(TIMER, PERIODIC | (IRQ_OFFSET + IRQ_TIMER));
    // 写入 initial-count 值为 0 时产生中断
    // 为零后会重新装入再次计数
    lapicw(TICR, 10000000);

    // Leave LINT0 of the BSP enabled so that it can get
    // interrupts from the 8259A chip.
    //
    // According to Intel MP Specification, the BIOS should initialize
    // BSP's local APIC in Virtual Wire Mode, in which 8259A's
    // INTR is virtually connected to BSP's LINTIN0. In this mode,
    // we do not need to program the IOAPIC.
    // 如果当前 CPU 不是 BSP 那么屏蔽来自 8259 PIC 的中断请求
    if (thiscpu != bootcpu)
        lapicw(LINT0, MASKED);

    // Disable NMI (LINT1) on all CPUs
    lapicw(LINT1, MASKED);

    // Disable performance counter overflow interrupts
    // on machines that provide that interrupt entry.
    if (((lapic[VER] >> 16) & 0xFF) >= 4)
        lapicw(PCINT, MASKED);

    // Map error interrupt to IRQ_ERROR.
    lapicw(ERROR, IRQ_OFFSET + IRQ_ERROR);

    // Clear error status register (requires back-to-back writes).
    lapicw(ESR, 0);
    lapicw(ESR, 0);

    // Ack any outstanding interrupts.
    lapicw(EOI, 0);

    // Send an Init Level De-Assert to synchronize arbitration ID's.
    lapicw(ICRHI, 0);
    lapicw(ICRLO, BCAST | INIT | LEVEL);
    while (lapic[ICRLO] & DELIVS)
    {
        ;
    }

    // Enable interrupts on the APIC (but not on the processor).
    // 允许响应所有的中断请求
    lapicw(TPR, 0);
}

// 从 local apic 里读取 cpu id
// lapic 是通过 MMIO 映射之后的地址，指向的是每个 CPU 自己的 local apic I/O 地址空间
// 并不是指向内存，看起来多个 CPU 都在访问同一个地址，实际访问的是不同的 I/O 硬件
int cpunum(void)
{
    uint32_t lapicid = 0;
    uint32_t cpuid = 0;
    if (lapic)
    {
        lapicid = lapic[ID] >> 24;
        for (int i = 0; i < ncpu; ++i)
        {
            if (cpus[i].apic_id == lapicid)
            {
                cpuid = i;
//                cprintf("cpunum lapic:%x, lapicid:%x, cpuid:%x\n", lapic, lapicid, cpuid);
                break;
            }
        }
        
        return cpuid;
    }
    
    return 0;
}

// Acknowledge interrupt.
void lapic_eoi(void)
{
    if (lapic)
    {
        lapicw(EOI, 0);
    }
}

// Spin for a given number of microseconds.
// On real hardware would want to tune this dynamically.
static void microdelay(int us)
{
    
}

// Start additional processor running entry code at addr.
// See Appendix B of MultiProcessor Specification.
/*
 中断命令寄存器（ICR）是一个 64 位本地 APIC 寄存器，允许运行在处理器上的软件指定和发送
 处理器间中断（IPI）给系统中的其它处理器。
 发送 IPI 时，必须设置 ICR 以指明将要发送的 IPI 消息的类型和目的处理器或处理器组
*/
void lapic_startap(uint8_t apicid, uint32_t addr)
{
    int i;
    uint16_t *wrv;

    // "The BSP must initialize CMOS shutdown code to 0AH
    // and the warm reset vector (DWORD based at 40:67) to point at
    // the AP startup code prior to the [universal startup algorithm]."
    outb(IO_RTC, 0xF); // offset 0xF is shutdown code
    outb(IO_RTC + 1, 0x0A);
    wrv = (uint16_t *)KADDR((0x40 << 4 | 0x67)); // Warm reset vector
    wrv[0] = 0;
    wrv[1] = addr >> 4;

    // "Universal startup algorithm."
    // Send INIT (level-triggered) interrupt to reset other CPU.
    // 将目标 CPU 的 ID 写入 ICR 寄存器的目的地址域中
    lapicw(ICRHI, apicid << 24);
    
    // 在 ASSERT 的情况下将 INIT 中断写入 ICR 寄存器
    lapicw(ICRLO, INIT | LEVEL | ASSERT);
    microdelay(200);
    
    // 在非 ASSERT 的情况下将 INIT 中断写入 ICR 寄存器
    lapicw(ICRLO, INIT | LEVEL);
    microdelay(100); // should be 10ms, but too slow in Bochs!

    // Send startup IPI (twice!) to enter code.
    // Regular hardware is supposed to only accept a STARTUP
    // when it is in the halted state due to an INIT.  So the second
    // should be ignored, but it is part of the official Intel algorithm.
    // Bochs complains about the second one.  Too bad for Bochs.
    // INTEL 官方规定发送两次 startup IPI 中断
    for (i = 0; i < 2; i++)
    {
        // 将目标 CPU 的 ID 写入 ICR 寄存器的目的地址域中
        lapicw(ICRHI, apicid << 24);
        // 将 SIPI 中断写入 ICR 寄存器的传送模式域中，将启动代码写入向量域中
        lapicw(ICRLO, STARTUP | (addr >> 12));
        microdelay(200);
    }
}

void lapic_ipi(int vector)
{
    lapicw(ICRLO, OTHERS | FIXED | vector);
    while (lapic[ICRLO] & DELIVS)
    {
        ;
    }
}

#define CMOS_PORT       0x70
#define CMOS_RETURN     0x71
#define CMOS_STATA      0x0a
#define CMOS_STATB      0x0b
#define CMOS_UIP        (1 << 7)        // RTC update in progress

#define SECS            0x00
#define MINS            0x02
#define HOURS           0x04
#define DAY             0x07
#define MONTH           0x08
#define YEAR            0x09

static uint32_t cmos_read(uint32_t reg)
{
    outb(CMOS_PORT,  reg);
    microdelay(200);
    
    return inb(CMOS_RETURN);
}

static void fill_rtcdate(struct rtcdate *r)
{
    r->second = cmos_read(SECS);
    r->minute = cmos_read(MINS);
    r->hour   = cmos_read(HOURS);
    r->day    = cmos_read(DAY);
    r->month  = cmos_read(MONTH);
    r->year   = cmos_read(YEAR);
}

// qemu seems to use 24-hour GWT and the values are BCD encoded
void cmos_time(struct rtcdate *r)
{
    struct rtcdate t1, t2;
    int sb, bcd;
    
    sb = cmos_read(CMOS_STATB);
    
    bcd = (sb & (1 << 2)) == 0;
    
    // make sure CMOS doesn't modify time while we read it
    for (;;)
    {
        fill_rtcdate(&t1);
        if (cmos_read(CMOS_STATA) & CMOS_UIP)
            continue;
        
        fill_rtcdate(&t2);
        if (memcmp(&t1, &t2, sizeof(t1)) == 0)
            break;
    }
    
    // convert
    if(bcd)
    {
#define    CONV(x)     (t1.x = ((t1.x >> 4) * 10) + (t1.x & 0xf))
        CONV(second);
        CONV(minute);
        CONV(hour);
        CONV(day);
        CONV(month);
        CONV(year);
#undef     CONV
    }
    
    *r = t1;
    r->year += 2000;
}


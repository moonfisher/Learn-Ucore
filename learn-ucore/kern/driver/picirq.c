#include "defs.h"
#include "x86.h"
#include "picirq.h"
#include "stdio.h"

// Current IRQ mask.
// Initial IRQ mask has interrupt 2 enabled (for slave 8259A).
uint16_t irq_mask_8259A = 0xFFFF & ~(1 << IRQ_SLAVE);
static bool did_init = 0;

/* pic_init - initialize the 8259A interrupt controllers */
/*
 端口地址参考 cat /proc/ioports
 20H-3FH　     可编程中断控制器1(8259)使用
 0A0H　　　     NM1屏蔽寄存器/可编程中断控制器2
 0A1H　　　     可编程中断控制器2屏蔽
 */
void pic_init(void)
{
    did_init = 1;

    // 重新映射 IRQ 表
    // 两片级联的 Intel 8259A 芯片
    // 主片端口 0x20 0x21
    // 从片端口 0xA0 0xA1

    // mask all interrupts
    outb(IO_PIC1 + 1, 0xFF);
    outb(IO_PIC2 + 1, 0xFF);

    // Set up master (8259A-1)

    // ICW1:  0001g0hi
    //    g:  0 = edge triggering, 1 = level triggering
    //    h:  0 = cascaded PICs, 1 = master only
    //    i:  0 = no ICW4, 1 = ICW4 required
    outb(IO_PIC1, 0x11);

    // ICW2:  Vector offset
    outb(IO_PIC1 + 1, IRQ_OFFSET);

    // ICW3:  (master PIC) bit mask of IR lines connected to slaves
    //        (slave PIC) 3-bit # of slave's connection to master
    outb(IO_PIC1 + 1, 1 << IRQ_SLAVE);

    // ICW4:  000nbmap
    //    n:  1 = special fully nested mode
    //    b:  1 = buffered mode
    //    m:  0 = slave PIC, 1 = master PIC
    //        (ignored when b is 0, as the master/slave role
    //         can be hardwired).
    //    a:  1 = Automatic EOI mode
    //    p:  0 = MCS-80/85 mode, 1 = intel x86 mode
    outb(IO_PIC1 + 1, 0x3);

    // Set up slave (8259A-2)
    outb(IO_PIC2, 0x11);               // ICW1
    outb(IO_PIC2 + 1, IRQ_OFFSET + 8); // ICW2
    outb(IO_PIC2 + 1, IRQ_SLAVE);      // ICW3
    // NB Automatic EOI mode doesn't tend to work on the slave.
    // Linux source code says it's "to be investigated".
    outb(IO_PIC2 + 1, 0x3); // ICW4

    // OCW3:  0ef01prs
    //   ef:  0x = NOP, 10 = clear specific mask, 11 = set specific mask
    //    p:  0 = no polling, 1 = polling mode
    //   rs:  0x = NOP, 10 = read IRR, 11 = read ISR
    outb(IO_PIC1, 0x68); // clear specific mask
    outb(IO_PIC1, 0x0a); // read IRR by default

    outb(IO_PIC2, 0x68); // OCW3
    outb(IO_PIC2, 0x0a); // OCW3

    if (irq_mask_8259A != 0xFFFF)
    {
        irq_setmask_8259A(irq_mask_8259A);
    }
}

void irq_setmask_8259A(uint16_t mask)
{
    int i;
    
    irq_mask_8259A = mask;
    if (!did_init)
        return;
    
    outb(IO_PIC1 + 1, (char)mask);
    outb(IO_PIC2 + 1, (char)(mask >> 8));
    cprintf("enabled interrupts:");
    
    for (i = 0; i < 16; i++)
        if (~mask & (1 << i))
            cprintf(" %d", i);
    
    cprintf("\n");
}

void irq_eoi(void)
{
    // OCW2: rse00xxx
    //   r: rotate
    //   s: specific
    //   e: end-of-interrupt
    // xxx: specific interrupt line
    outb(IO_PIC1, 0x20);
    outb(IO_PIC2, 0x20);
}

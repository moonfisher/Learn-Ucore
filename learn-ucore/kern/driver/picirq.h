#ifndef __KERN_DRIVER_PICIRQ_H__
#define __KERN_DRIVER_PICIRQ_H__

#include "defs.h"
#include "x86.h"

#define MAX_IRQS    16      // Number of IRQs
#define IRQ_OFFSET  32

// I/O Addresses of the two 8259A programmable interrupt controllers
/*
 端口地址参考 cat /proc/ioports
 20H-3FH　     可编程中断控制器1(8259)使用
 0A0H　　　     NM1屏蔽寄存器/可编程中断控制器2
 0A1H　　　     可编程中断控制器2屏蔽
*/
#define IO_PIC1     0x20    // Master (IRQs 0-7)
#define IO_PIC2     0xA0    // Slave (IRQs 8-15)

#define IRQ_SLAVE   2       // IRQ at which slave connects to master

extern uint16_t irq_mask_8259A;
void pic_init(void);
void pic_enable(unsigned int irq);
void irq_setmask_8259A(uint16_t mask);
void irq_eoi(void);

#endif /* !__KERN_DRIVER_PICIRQ_H__ */

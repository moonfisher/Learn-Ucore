#include "x86.h"
#include "trap.h"
#include "stdio.h"
#include "picirq.h"

/* *
 * Support for time-related hardware gadgets - the 8253 timer,
 * which generates interruptes on IRQ-0.
 * */
/*
 端口地址参考 cat /proc/ioports
 40H　　　      可编程中断计时器(8253)使用，读/写计数器0
 41H　　　      可编程中断计时器寄存器
 42H　　　      可编程中断计时器杂项寄存器
 43H　　　      可编程中断计时器,控制字寄存器
 44H　　　      可编程中断计时器,杂项寄存器（AT）
 47H　　　      可编程中断计时器,计数器0的控制字寄存器
 48H-5FH       可编程中断计时器使用
*/
#define IO_TIMER1       0x040               // 8253 Timer #1

/* *
 * Frequency of all three count-down timers; (TIMER_FREQ/freq)
 * is the appropriate count to generate a frequency of freq Hz.
 * */

#define TIMER_FREQ      1193182
#define TIMER_DIV(x)    ((TIMER_FREQ + (x) / 2) / (x))

#define TIMER_MODE      (IO_TIMER1 + 3)         // timer mode port
#define TIMER_SEL0      0x00                    // select counter 0
#define TIMER_RATEGEN   0x04                    // mode 2, rate generator
#define TIMER_16BIT     0x30                    // r/w counter 16 bits, LSB first

volatile size_t ticks;

long SYSTEM_READ_TIMER( void )
{
    return ticks;
}

long gettime(long* store)
{
    if (store)
    {
       *store = ticks;
    }
	return ticks;
}

long gettime2()
{
    return ticks;
}

/* *
 * clock_init - initialize 8253 clock to interrupt 100 times per second,
 * and then enable IRQ_TIMER.
 * */
void clock_init(void)
{
    // set 8253 timer-chip
    outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
    outb(IO_TIMER1, TIMER_DIV(100) % 256);
    outb(IO_TIMER1, TIMER_DIV(100) / 256);

    // initialize time counter 'ticks' to zero
    ticks = 0;

    cprintf("++ setup timer interrupts\n");
    irq_setmask_8259A(irq_mask_8259A & ~(1 << IRQ_TIMER));
}


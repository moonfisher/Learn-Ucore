#include "x86.h"
#include "intr.h"

/* intr_enable - enable irq interrupt */
void local_irq_enable(void)
{
    sti();
}

/* intr_disable - disable irq interrupt */
void local_irq_disable(void)
{
    cli();
}


#include "defs.h"
#include "stdio.h"
#include "intr.h"
#include "kmonitor.h"
#include "cpu.h"

void print_stackframe(void);
static bool is_panic = 0;

/* *
 * __panic - __panic is called on unresolvable fatal errors. it prints
 * "panic: 'message'", and then enters the kernel monitor.
 * */
// 如果发生严重错误，就打开控制台，供用户调试
void __panic(const char *file, int line, const char *fmt, ...)
{
    if (is_panic)
    {
        goto panic_dead;
    }
    is_panic = 1;

    int cid = thiscpu->cpu_id;
    // print the 'message'
    va_list ap;
    va_start(ap, fmt);
    cprintf("kernel panic on cpu:%d at %s:%d: \n    ", cid, file, line);
    vcprintf(fmt, ap);
    cprintf("\n");

    cprintf("stack trackback:\n");
    print_stackframe();

    va_end(ap);

panic_dead:
    local_irq_disable();
    while (1)
    {
        kmonitor(NULL);
    }
}

/* __warn - like panic, but don't */
void __warn(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    cprintf("kernel warning at %s:%d:\n    ", file, line);
    vcprintf(fmt, ap);
    cprintf("\n");
    va_end(ap);
}

bool is_kernel_panic(void)
{
    return is_panic;
}


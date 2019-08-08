#ifndef SETJMP_H
#define SETJMP_H

#include "defs.h"

#define LONGJMP_GCCATTR regparm(2)

struct jmp_buf
{
    uint32_t jb_eip;
    uint32_t jb_esp;
    uint32_t jb_ebp;
    uint32_t jb_ebx;
    uint32_t jb_esi;
    uint32_t jb_edi;
};

int setjmp(volatile struct jmp_buf *buf);
void longjmp(volatile struct jmp_buf *buf, int val) __attribute__((__noreturn__, LONGJMP_GCCATTR));

#endif

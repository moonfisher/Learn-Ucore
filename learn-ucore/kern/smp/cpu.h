
#ifndef CPU_H
#define CPU_H

#include "defs.h"
#include "memlayout.h"
#include "mmu.h"
//#include "env.h"

// Maximum number of CPUs
#define NCPU    8

// Values of status in struct Cpu
enum
{
    CPU_UNUSED = 0,
    CPU_STARTED,
    CPU_HALTED,
};

// Per-CPU state
struct cpu_info
{
    uint8_t cpu_id;                 // index into cpus[] below
    uint8_t apic_id;                // Local APIC ID
    volatile unsigned cpu_status;   // The status of the CPU
//    struct Env *cpu_env;          // The currently-running environment.
    struct taskstate cpu_ts;        // Used by x86 to find stack for interrupt
};

// Initialized in mpconfig.c
extern struct cpu_info cpus[NCPU];
extern int ncpu;				// Total number of CPUs in the system
extern struct cpu_info *bootcpu; // The boot-strap processor (BSP)
extern physaddr_t lapicaddr;	// Physical MMIO address of the local APIC
extern unsigned char ioapicid;

// Per-CPU kernel stacks
extern unsigned char percpu_kstacks[NCPU][KSTACKSIZE];

int cpunum(void);

#define thiscpu (&cpus[cpunum()])

void mp_init(void);
void lapic_init(void);
void lapic_startap(uint8_t apicid, uint32_t addr);
void lapic_eoi(void);
void lapic_ipi(int vector);

#endif

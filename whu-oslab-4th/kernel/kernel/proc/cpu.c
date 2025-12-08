#include "proc/cpu.h"
#include "lock/lock.h"
#include "riscv.h"

cpu_t cpus[NCPU];

cpu_t* mycpu(void) 
{
    int hartid = r_tp();
    return &cpus[hartid];
}

proc_t* myproc(void) 
{
    push_off();
    proc_t* p = mycpu()->myproc;
    pop_off();
    return p;
}

int mycpuid(void) {
    int hartid = r_tp();
    return hartid;
}

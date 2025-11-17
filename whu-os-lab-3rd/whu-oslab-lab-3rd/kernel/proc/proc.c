#include "proc/proc.h"
#include "riscv.h"

static cpu_t cpus[NCPU];

// 返回当前 CPU 的结构体指针
cpu_t* mycpu(void)
{
    int id = mycpuid();
    cpu_t *c = &cpus[id];
    return c;
}

int mycpuid(void) 
{
    int id = r_tp();
    return id;
}

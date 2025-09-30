#include "proc/proc.h"
#include "riscv.h"

static cpu_t cpus[NCPU];

// 返回当前 CPU 的结构体指针
cpu_t* mycpu(void)
{
    int id;
    // 读取当前 hartid（即 CPU id）
    asm volatile("mv %0, tp" : "=r"(id));
    return &cpus[id];
}

// 返回当前 CPU 的 id
int mycpuid(void) 
{
    int id;
    asm volatile("mv %0, tp" : "=r"(id));
    return id;
}

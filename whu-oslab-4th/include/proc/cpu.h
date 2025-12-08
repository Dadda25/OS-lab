#ifndef __CPU_H__
#define __CPU_H__

#include "proc/proc.h"

typedef struct cpu {
    proc_t* myproc; // 当前运行的proc
    context_t ctx;  // 调度器的上下文(proc_schedule中)
    int noff;       // push_off的深度
    int origin;     // push_off之前中断是打开的吗
} cpu_t;

int     mycpuid(void);
cpu_t*  mycpu(void);
proc_t* myproc(void);

#endif
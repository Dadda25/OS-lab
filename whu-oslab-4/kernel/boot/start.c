#include "riscv.h"
#include "memlayout.h"
#include "dev/uart.h"
#include "dev/timer.h"

// 机器启动流程：open-sbi(M-mode) -> entry.S(S-mode) -> start.c -> main.c

// 操作系统在内核的栈空间(每个核心占KSTACK_SIZE个字节)
__attribute__ ((aligned (16))) char kernel_stack[4096 * NCPU];

extern void main();

void trap_loop()
{
    while(1);
}

// 由 open-sbi 传递 hartid 和 设备树信息(暂时不使用)
void start(uint64 hartid, uint64 dtb_entry)
{
    // 不进行分页(使用物理内存)
    w_satp(0);
        
    // 使能S态的外设中断和时钟中断 (暂时不使用软件中断)
    w_sie(r_sie() | SIE_SEIE | SIE_STIE);

    // trap响应程序设为死循环
    w_stvec((uint64)trap_loop);

    // 使用tp保存hartid以方便在S态查看
    w_tp(hartid);

    // 进入main函数完成一系列初始化
    main();
}
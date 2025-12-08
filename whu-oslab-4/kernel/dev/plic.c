// Platform-level interrupt controller

#include "riscv.h"
#include "memlayout.h"
#include "dev/plic.h"
#include "lib/print.h"

void plic_init()
{
    // 设置中断优先级
    *(uint32*)(PLIC_PRIORITY(UART_IRQ)) = 1;
    *(uint32*)(PLIC_PRIORITY(VIO_IRQ)) = 1;
}

void plic_inithart()
{   
    int hart = (int)r_tp();
    // 使能中断开关
    *(uint32*)PLIC_SENABLE(hart) = (1 << UART_IRQ) | (1 << VIO_IRQ);
    // 设置响应阈值
    *(uint32*)PLIC_SPRIORITY(hart) = 0;
}

// 获取中断号
int plic_claim(void)
{
    int hart = (int)r_tp();
    int irq = *(uint32*)PLIC_SCLAIM(hart);
    return irq;
}

// 确认该中断号对应中断已经完成
void plic_complete(int irq)
{
    int hart = (int)r_tp();
    *(uint32*)PLIC_SCLAIM(hart) = irq;
}
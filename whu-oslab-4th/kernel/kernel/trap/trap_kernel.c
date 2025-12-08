#include "lib/print.h"
#include "dev/plic.h"
#include "dev/uart.h"
#include "dev/timer.h"
#include "dev/vio.h"
#include "proc/cpu.h"
#include "trap/trap.h"
#include "memlayout.h"
#include "riscv.h"

// trap全局初始化
void trap_init(void)
{
    timer_init();
}

// trap核心初始化
void trap_inithart(void)
{
    w_stvec((uint64)trap_vector);
    timer_setNext(true);
    intr_on(); // S态总中断开启
}

// 外设中断处理
void external_interrupt_handler()
{
   // printf("External interrupt received!\n");
    int irq = plic_claim();
    switch(irq) {
        case UART_IRQ:
       // printf("UART interrupt received!\n");
            uart_intr();
            break;
        case VIO_IRQ:
            virtio_disk_intr();
            break;
        default:
            panic("unkown irq\n");
            break;
    }
    if(irq) plic_complete(irq);
}

// 时钟中断处理
// 修改 timer.c 中的 timer_interrupt_handler
void timer_interrupt_handler(bool inkernel) {
    // printf("Clock interrupt triggered!\n"); // 新增打印
    // printf("ticks ouput: %d\n",ticks);
    printf("T");
    timer_setNext(true);     // 更新ticks
    proc_wakeup(&ticks);
}
// 由trap.S调用，处理内核态遇到的trap
void trap_kernel()
{
    // 此时是S-mode
    // reg sepc = r_sepc();
    reg sstatus = r_sstatus();
    reg cause = r_scause();
    uint64 cause_code = cause & 0xf;

    if((sstatus & SSTATUS_SPP) == 0)
        panic("trap_kernel: not from S-mode\n");
    if(intr_get() != 0)
        panic("trap_kernel: interrupts enabled\n");

    if(cause & 0x8000000000000000) { // interrupt
        switch (cause_code) {
            case 5: // 时钟中断
                timer_interrupt_handler(true);
                break;
            case 9: // 外部中断
                external_interrupt_handler();
                break;
            default:
                printf("code = %uld\n",cause_code);
                panic("trap_kernel: Unknow Kernel Interrupt!\n");
        }
    } else { // exception
        //printf("code = %uld\n",cause_code);
        printf("Kernel Exception! scause=%p stval=%p\n", r_scause(), r_stval());
        panic("trap_kernel: Unknow Kernel Exception!\n");
    }
    // w_sepc(sepc);
    // w_sstatus(sstatus);
}
#include "lib/print.h"
#include "trap/trap.h"
#include "proc/cpu.h"
#include "mem/vmem.h"
#include "syscall/syscall.h"
#include "riscv.h"

// Trampoline.S中的一些Label
extern char trampoline[],uservec[],userret[];

/*
    从内核态返回用户态
    与trampoline.S密切配合
*/
void trapret_user()
{
    proc_t* p = myproc();

    intr_off();
    
    uint64 uservec_va = TRAMPOLINE + (uservec - trampoline);
    w_stvec(uservec_va);

    // 在Trampoline.S中用到(当下次进入内核时需要这些信息)
    p->tf->kernel_satp = r_satp();             // 内核页表
    p->tf->kernel_sp = p->kstack + PAGE_SIZE;  // 内核栈
    p->tf->kernel_trap = (uint64)trap_user;    // 内核中trap处理
    p->tf->kernel_hartid = (uint64)mycpuid();  

    // 修改sstatus寄存器
    reg sstatus = r_sstatus();
    sstatus &= ~SSTATUS_SPP; // clear SSP
    sstatus |= SSTATUS_SPIE; // enable SPIE
    w_sstatus(sstatus);

    // 如果发生的是系统调用,p->tf->epc会被更新,这里需要写回
    w_sepc(p->tf->epc);

    // 准备参数pagetable,然后调用trampoline.S中的userret(使用它的虚拟地址)
    uint64 satp =  MAKE_SATP(p->pagetable);
    uint64 userret_va = TRAMPOLINE + (userret - trampoline);
    
    ((void(*)(uint64))userret_va)(satp);
}

/*
    处理用户态中断,异常,系统调用
    在trampoline.S中被调用
*/
void trap_user(void)
{
    // 上一个状态应当是U-mode
    assert((r_sstatus() & SSTATUS_SPP) == 0 , "trap_user.c->trap_user");
    // 由于进入了S-mode,需要切换trap处理函数
    w_stvec((uint64)trap_vector);

    proc_t* p = myproc();
    p->tf->epc = r_sepc();

    // 根据触发trap的原因分类讨论
    reg scause = r_scause();
    uint64 cause_code  = scause & 0xF;
    if(scause & 0x8000000000000000) { // 中断
        switch (cause_code) {
            case 5: // 时钟中断
                timer_interrupt_handler(false);
                break;
            case 9: // 外设中断
                external_interrupt_handler();
                break;
            default:
                printf("Unknow User Interrupt! Code = %uld\n",cause_code);
                proc_setkilled(p);
        }
    } else { // 异常
        switch (cause_code) {
            case 8: //syscall
                printf("[trap_user] get a syscall from pid=%d\n", p->pid);
                if(proc_iskilled(p))
                    proc_exit(-1);
                p->tf->epc += 4;
                intr_on();
                syscall();
                break;
            default:
                printf("stval = %p\n", r_stval());
                printf("Unknow User Exception! Code = %uld\n",cause_code);
                proc_setkilled(p);
        }
    }
    if(proc_iskilled(p))
        proc_exit(-1);
    trapret_user();
}
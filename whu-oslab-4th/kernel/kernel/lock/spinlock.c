#include "lock/lock.h"
#include "proc/cpu.h"
#include "lib/print.h"
#include "riscv.h"

// 带层数叠加的关中断
void push_off(void)
{
    int old = intr_get();
    intr_off();
    if(mycpu()->noff == 0) 
        mycpu()->origin = old;
    mycpu()->noff++; 
}

// 带层数叠加的开中断
// 只有所有的关中断指令都被弹出时，才真正恢复中断原来的样子
void pop_off(void)
{
    cpu_t* cpu = mycpu();
    assert(intr_get() == 0, "spinlock.c->push_off: 1\n");
    assert(cpu->noff >= 1, "spinlock.c->push_off: 2\n");
    cpu->noff--;
    if(cpu->noff == 0 && cpu->origin == 1)
        intr_on();
}

bool spinlock_holding(spinlock_t *lk)
{
    return ((lk->locked == 1) && (lk->cpuid == mycpuid()));
}

void spinlock_init(spinlock_t *lk,char *name)
{
    lk->name = name;
    lk->locked = 0;
    lk->cpuid = -1;
}

void spinlock_acquire(spinlock_t *lk)
{    
    push_off();
    assert(!spinlock_holding(lk), "spinlock.c->acquire: 1\n");
    // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
    // a5 = 1
    // s1 = &lk->locked
    // amoswap.w.aq a5, a5, (s1)
    while (__sync_lock_test_and_set(&lk->locked, 1) != 0);
    __sync_synchronize();
    lk->cpuid = mycpuid();
} 

void spinlock_release(spinlock_t *lk)
{
    assert(spinlock_holding(lk), "spinlock.c->release: 1\n");
    lk->cpuid = -1;
    __sync_synchronize();
    // On RISC-V, sync_lock_release turns into an atomic swap:
    // s1 = &lk->locked
    // amoswap.w zero, zero, (s1)
    __sync_lock_release(&lk->locked);
    pop_off();
}
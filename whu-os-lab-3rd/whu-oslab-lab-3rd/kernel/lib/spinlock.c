#include "lib/lock.h"
#include "lib/print.h"
#include "proc/proc.h"
#include "riscv.h"


int holding(struct spinlock *lk)
{
    int r;
    r = (lk->locked && lk->cpuid == mycpuid());
    return r;
}

// 带层数叠加的关中断
void pop_off(void)
{
    cpu_t *c = mycpu();
    if (c->noff <= 0)
        panic("pop_off: unmatched pop_off");
    c->noff -= 1;          // 减少嵌套层数
    if (c->noff == 0 && c->origin)
        intr_on();         // 恢复第一次保存的中断状态
}

// 带层数叠加的开中断
void push_off(void)
{
    cpu_t *c = mycpu();
    int old = intr_get();  // 保存当前中断状态
    intr_off();            // 关闭中断
    if (c->noff == 0)      // 第一次关中断时保存原状态
        c->origin = old;
    c->noff += 1;          // 增加嵌套层数
}

// 是否持有自旋锁
// 中断应当是关闭的
bool spinlock_holding(spinlock_t *lk)
{
    int r;
    r = (lk->locked && lk->cpuid == mycpuid());
    return r;
}

// 自选锁初始化
void spinlock_init(spinlock_t *lk, char *name)
{
    lk->name = name;
    lk->locked = 0;
    lk->cpuid = -1;
}

// 获取自选锁
void spinlock_acquire(spinlock_t *lk)
{    
    push_off(); // disable interrupts to avoid deadlock.
    if(holding(lk)) panic("acquire");

    while(__sync_lock_test_and_set(&lk->locked, 1) != 0);

    __sync_synchronize();

    lk->cpuid = mycpuid();
} 

// 释放自旋锁
void spinlock_release(spinlock_t *lk)
{
    if(!holding(lk)) panic("release");

    lk->cpuid = -1;

    __sync_synchronize();

    __sync_lock_release(&lk->locked);

    pop_off();
}
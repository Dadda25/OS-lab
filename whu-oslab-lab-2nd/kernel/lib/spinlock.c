#include "lib/lock.h"
#include "lib/print.h"
#include "proc/proc.h"
#include "riscv.h"

// 带层数叠加的关中断
void push_off(void)
{
    // int old = intr_get();  // 删除未使用变量
    intr_off();            // 关闭中断
}

// 带层数叠加的开中断
void pop_off(void)
{
    // 恢复之前的中断状态
    intr_on(); // 简单实现，恢复中断
}

// 是否持有自旋锁
// 中断应当是关闭的
bool spinlock_holding(spinlock_t *lk)
{
    // 用 mycpuid() 替换 cpuid()
    if (lk->locked && lk->cpuid == mycpuid()) {
        return true;
    }
    return false;
}

// 自旋锁初始化
void spinlock_init(spinlock_t *lk, char *name)
{
    lk->locked = 0;
    lk->name = name;
    lk->cpuid = -1;
}

// 获取自旋锁
void spinlock_acquire(spinlock_t *lk)
{
    push_off(); // 关闭中断，防止死锁

    while (__sync_lock_test_and_set(&lk->locked, 1) != 0) {
        // 自旋等待
    }

    // 锁定成功
    lk->cpuid = mycpuid();
}

// 释放自旋锁
void spinlock_release(spinlock_t *lk)
{
    if (!spinlock_holding(lk)) {
        panic("spinlock_release: not holding lock");
    }

    lk->cpuid = -1;
    __sync_lock_release(&lk->locked);

    pop_off(); // 恢复中断状态
}

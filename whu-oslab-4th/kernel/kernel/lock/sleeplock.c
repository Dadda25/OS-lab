#include "proc/cpu.h"
#include "lock/lock.h"

void sleeplock_init(sleeplock_t* lock, char* name)
{
    spinlock_init(&lock->lk, "sleep lock");
    lock->name = name;
    lock->locked = 0;
    lock->pid = 0;
}

void sleeplock_acquire(sleeplock_t* lock) 
{
    spinlock_acquire(&lock->lk);
    while(lock->locked)
        proc_sleep(lock,&lock->lk);
    lock->locked = 1;
    lock->pid = myproc()->pid;
    spinlock_release(&lock->lk);
}

void sleeplock_release(sleeplock_t* lock) 
{
    spinlock_acquire(&lock->lk);
    lock->locked = 0;
    lock->pid = 0;
    proc_wakeup(lock);
    spinlock_release(&lock->lk);
}

bool sleeplock_holding(sleeplock_t* lock)
{
    bool flag;
    spinlock_acquire(&lock->lk);
    flag = (lock->locked && (lock->pid == myproc()->pid));
    spinlock_release(&lock->lk);
    return flag;
}
#ifndef __LOCK_H__
#define __LOCK_H__

#include "common.h"

typedef struct spinlock {
    int locked;  // 是否上锁
    char* name;  // for debug
    int cpuid;  // 哪个CPU持有这个锁
} spinlock_t;

typedef struct sleeplock {
    int locked;    // 是否上锁
    int pid;       // 持有锁的进程号
    spinlock_t lk; // 保护locked
    char* name;    // for debug
}sleeplock_t;

void push_off();
void pop_off();

void spinlock_init(spinlock_t* lk,char* name);
void spinlock_acquire(spinlock_t* lk);
void spinlock_release(spinlock_t* lk);
bool spinlock_holding(spinlock_t* lk); 

void sleeplock_init(sleeplock_t* lock, char* name);
void sleeplock_acquire(sleeplock_t* lock);
void sleeplock_release(sleeplock_t* lock);
bool sleeplock_holding(sleeplock_t* lock);

#endif
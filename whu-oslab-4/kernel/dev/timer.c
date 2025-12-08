#include "memlayout.h"
#include "sbi.h"
#include "riscv.h"
#include "dev/timer.h"
#include "lib/print.h"

uint64 ticks;
spinlock_t ticks_lk;
// 获取硬件时钟(自启动以来的总滴答数)
// 一切时钟函数的基础
uint64 timer_mono_clock()
{
    uint64 n;
    asm volatile("rdtime %0" : "=r"(n));
    return n;
}

// 获取实时时钟
uint64 timer_rtc_clock()
{
    return timer_mono_clock() + (55 * 365 + 200) * 24 * 3600 * CLOCK_PER_SEC;
}

void timer_init()
{
    spinlock_init(&ticks_lk, "ticks");
    ticks = 0;
}

void timer_setNext(bool update)
{
    // 设置计时器，再过INTERVAL个滴答后触发时钟中断
    SBI_SET_TIMER(timer_mono_clock() + INTERVAL);
    // 更新ticks
    if(update) {
        spinlock_acquire(&ticks_lk);
        ticks++;
        spinlock_release(&ticks_lk);
    }
}

timeval_t timer_get_tv()
{
    timeval_t tv;
    uint64 clk = timer_rtc_clock();
    tv.sec = CLOCK_TO_SEC(clk);
    tv.usec = CLOCK_TO_USEC(clk) % USEC_PER_SEC;
    return tv;
}

#define CT_REALTIME  0   // 实时时钟
#define CT_MONOTONIC 1   // 累加时钟

timespec_t timer_get_ts(int clock_type)
{
    timespec_t ts;
    uint64 clocks = 0;
    if(clock_type == CT_REALTIME) {
        clocks = timer_rtc_clock();
    } else if(clock_type == CT_MONOTONIC) {
        clocks = timer_mono_clock();
    } else {
        panic("timer_get_ts: 0");
    }
    ts.sec = CLOCK_TO_SEC(clocks);
    ts.nsec = CLOCK_TO_NSEC(clocks) % NSEC_PER_SEC;
    return ts;
}
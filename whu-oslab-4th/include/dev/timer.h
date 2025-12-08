#ifndef __TIMER_H__
#define __TIMER_H__

#include "lock/lock.h"

extern uint64 ticks;
extern spinlock_t ticks_lk;
// 时间间隔
typedef struct timeval {
    uint64 sec;      // 秒
    uint64 usec;     // 微秒
} timeval_t;

// 时间
typedef struct timespec {
	uint64 sec;    // 秒
	uint64 nsec;   // 纳秒
} timespec_t;


#define USEC_PER_SEC 1000000ul    // 1s = ? us
#define NSEC_PER_SEC 1000000000ul // 1s = ? ns

#define CLOCK_FREQ          10000000ul                     // 时钟滴答的频率

#define CLOCK_PER_SEC       CLOCK_FREQ                     // 每秒时钟滴答次数
#define CLOCK_PER_USEC      (CLOCK_PER_SEC / USEC_PER_SEC) // 每微秒时钟滴答次数

#define CLOCK_TO_SEC(clk)   ((clk)/CLOCK_PER_SEC)                // 滴答次数 => 过了多少秒
#define CLOCK_TO_USEC(clk)  ((clk)/CLOCK_PER_USEC)               // 滴答次数 => 过了多少微秒        
#define CLOCK_TO_NSEC(clk)  ((clk)*(NSEC_PER_SEC/CLOCK_FREQ))    // 滴答次数 => 过了多少纳秒

// #define INTERVAL (CLOCK_PER_SEC / 100)                     // 时钟中断的间隔滴答数(0.1s)
#define INTERVAL  10000000                          // 时钟中断的间隔滴答数(0.1s)
uint64 timer_mono_clock();                                 // 获取自启动以来的滴答数
uint64 timer_rtc_clock();                                  // 获取自linux起源以来的滴答数

void   timer_init();                                       // 时钟初始化
void   timer_setNext(bool update);                         // 计时器到期后设置下一个

timeval_t  timer_get_tv();
timespec_t timer_get_ts(int clock_type);

#endif
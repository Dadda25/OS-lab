#include "dev/rtc.h"
#include "memlayout.h"

#define R(r) ((volatile uint32*)(RTC_BASE + (r)))


uint64 rtc_gettime()
{
    uint64 time_high, time_low, time;
    time_low  = *R(TIMER_TIME_LOW);
    time_high = *R(TIMER_TIME_HIGH);
    time = (time_high << 32) | time_low;
    return time;
}
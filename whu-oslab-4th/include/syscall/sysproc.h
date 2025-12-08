// 关于进程管理的syscall
#ifndef __SYSPROC_H__
#define __SYSPROC_H__

#include "common.h"

uint64 sys_clone();
uint64 sys_execve();
uint64 sys_wait4();
uint64 sys_exit();
uint64 sys_exit_group();
uint64 sys_getppid();
uint64 sys_getpid();
uint64 sys_getuid();
uint64 sys_geteuid();
uint64 sys_getegid();
uint64 sys_gettid();
uint64 sys_set_tid_address();
uint64 sys_sched_yield();
uint64 sys_nanosleep();
uint64 sys_kill();

uint64 sys_brk();
uint64 sys_munmap();       
uint64 sys_mmap();
uint64 sys_mprotect();
uint64 sys_madvice();     

uint64 sys_times();
uint64 sys_gettimeofday();
uint64 sys_clock_gettime();
uint64 sys_sysinfo();
uint64 sys_syslog();
uint64 sys_uname();

uint64 sys_rt_sigaction();
uint64 sys_rt_sigprocmask();
uint64 sys_rt_sigtimedwait();
uint64 sys_rt_sigreturn();

uint64 sys_prlimit();
uint64 sys_print(void);

uint64 sys_shutdown();
#endif
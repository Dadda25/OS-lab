#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include "common.h"

// 系统调用主处理函数
void syscall(void);
// 基于addr的读取
int fetch_addr(uint64 addr, uint64* ip);
int fetch_str(uint64 addr, char* buf, int maxlen);
// 基于参数寄存器编号n的读取
void arg_int(int n, int* ip);
void arg_addr(int n, uint64* ip);
int arg_str(int n, char* buf, int maxlen);

#endif
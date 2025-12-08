#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include "lock/lock.h"

#define INPUT_BUF_SIZE 128

typedef struct console{
    spinlock_t lk;
    char buf[INPUT_BUF_SIZE];
    uint32 r;   // read index
    uint32 w;   // write index
    uint32 e;   // edit index
} console_t;

void cons_init();                // console初始化      
void cons_putc(int c);           // 单个字符输出
void cons_puts(char* s);         // 字符串输出
void cons_intr(int c);           // 输入中断响应

#define CONSOLE 1   // console设备的主设备号
#define DEVNULL 2   // /dev/null设备的主设备号
#define DEVZERO 3   // /dev/zero设备的主设备号
 
// 这两个函数用于设备文件读写
int cons_read(bool user_src, uint64 dst, uint32 n);
int cons_write(bool user_src, uint64 src, uint32 n);

int null_read(bool user_src, uint64 dst, uint32 n);
int null_write(bool user_src, uint64 src, uint32 n);

int zero_read(bool user_dst, uint64 dst, uint32 n);
#endif
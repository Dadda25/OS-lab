// 这个头文件通常认为其他.h文件都应该include
#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdbool.h>
#include <stdint.h>

// 类型定义
typedef char                   int8;
typedef short                  int16;
typedef int                    int32;
typedef long long              int64;
typedef unsigned char          uint8; 
typedef unsigned short         uint16;
typedef unsigned int           uint32;
typedef unsigned long long     uint64;

typedef unsigned long long     reg; 
//typedef enum {false = 0, true = 1} bool;

#ifndef NULL
#define NULL ((void*)0)
#endif

#define NCPU 2

// 页面大小定义
#define PGSIZE 4096   // 4KB页面大小
#define PGSHIFT 12    // log2(PGSIZE)

// 页面对齐宏
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

#endif
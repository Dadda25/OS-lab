#ifndef __PMEM_H__
#define __PMEM_H__

#include "common.h"

// 来自 mem.S 和 pmem.c
extern uint64 KERNEL_TEXT;
extern uint64 KERNEL_DATA;
extern uint64 USER_BASE;
extern uint64 USER_END;


void  pmem_init(bool output);                                    // 物理内存初始化
void* pmem_alloc_pages(int npages, bool in_kernel);              // 物理页申请
void  pmem_free_pages(void* ptr, int npages, bool in_kernel);    // 物理页释放

#endif
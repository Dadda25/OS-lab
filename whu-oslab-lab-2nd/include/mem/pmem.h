#ifndef __PMEM_H__
#define __PMEM_H__

#include "common.h"

// 物理页大小
#define PGSIZE 4096

// 物理内存上限（可以按实际开发板修改）
#ifndef PHYSTOP
#define PHYSTOP (KERNEL_BASE + 128*1024*1024)  // 128MB 物理内存
#endif

// 空闲链表节点
struct run {
    struct run *next;
};

// 物理内存统计信息
struct pmem_stat {
    uint64 total_pages;
    uint64 free_pages;
    uint64 used_pages;
};

// API
void   pmem_init(void);
void*  pmem_alloc(bool in_kernel);
void   pmem_free(uint64 pa, bool in_kernel);
void*  pmem_alloc_pages(int n, bool in_kernel);
int    pmem_free_pages_count(void);
void   pmem_get_stat(struct pmem_stat *stat);

#endif /* __PMEM_H__ */

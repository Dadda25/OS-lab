#ifndef __KVM_H__
#define __KVM_H__

#include "common.h"
#include "riscv.h"
#include "vmem.h"

// 页表管理函数

// 创建新的页表
pgtbl_t create_pagetable(void);

// 释放页表
void destroy_pagetable(pgtbl_t pagetable);

// 映射单个页面
int map_page(pgtbl_t pagetable, uint64 va, uint64 pa, int perm);

// 映射一段连续内存
int map_pages(pgtbl_t pagetable, uint64 va, uint64 size, uint64 pa, int perm);

// 取消映射
void unmap_pages(pgtbl_t pagetable, uint64 va, uint64 npages, int do_free);

// 页表遍历
pte_t* walk(pgtbl_t pagetable, uint64 va, int alloc);

// 虚拟地址转物理地址
uint64 walkaddr(pgtbl_t pagetable, uint64 va);

// 打印页表
void dump_pagetable(pgtbl_t pt, int level);  

// 页表映射函数
int map_region(pgtbl_t pt, uint64 va, uint64 pa, uint64 size, int perm);

// 内核页表管理
void kvminit(void);
void kvminithart(void);

// 调试函数
void vmprint(pgtbl_t pagetable);

// 全局内核页表
extern pgtbl_t kernel_pagetable;

#endif
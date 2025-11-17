#ifndef __VMEM_H__
#define __VMEM_H__

#include "common.h"
#include "riscv.h"
#include "lib/str.h"   // memset
#include "lib/lock.h"  
#include "lib/print.h"

// 页表项
typedef uint64 pte_t;

// 顶级页表
typedef uint64* pgtbl_t;

// satp寄存器相关
#define SATP_SV39 (8L << 60)  // MODE = SV39
#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12)) // 设置MODE和PPN字段

// 获取虚拟地址中的虚拟页(VPN)信息 占9bit
#define VA_SHIFT(level)         (12 + 9 * (level))
#define VA_TO_VPN(va,level)     ((((uint64)(va)) >> VA_SHIFT(level)) & 0x1FF)

// PA和PTE之间的转换
#define PA_TO_PTE(pa) ((((uint64)(pa)) >> 12) << 10)
#define PTE_TO_PA(pte) (((pte) >> 10) << 12)

#define PTE_G (1 << 5) // global
#define PTE_A (1 << 6) // accessed
#define PTE_D (1 << 7) // dirty

// 检查一个PTE是否属于pgtbl
#define PTE_CHECK(pte) (((pte) & (PTE_R | PTE_W | PTE_X)) == 0)

// 获取低10bit的flag信息
#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// 定义一个相当大的VA, 规定所有VA不得大于它
#define VA_MAX (1ul << 38)

void   vm_print(pgtbl_t pgtbl);
pte_t* vm_getpte(pgtbl_t pgtbl, uint64 va, bool alloc);
void   vm_mappages(pgtbl_t pgtbl, uint64 va, uint64 pa, uint64 len, int perm);
void   vm_unmappages(pgtbl_t pgtbl, uint64 va, uint64 len, bool freeit);

void   kvm_init();
void   kvm_inithart();

#endif
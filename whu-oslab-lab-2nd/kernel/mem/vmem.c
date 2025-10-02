#include "mem/vmem.h"
#include "mem/pmem.h"
#include "lib/str.h"
#include "dev/uart.h"
#include "riscv.h"
#include "memlayout.h"
#include "lib/print.h"

// static pgtbl_t kernel_pgtbl = 0;

// 内部函数：分配一个零填充的页表
static pgtbl_t alloc_pgtbl(void) {
    void* pa = pmem_alloc(true);
    if (!pa) {
        uart_puts("[vmem] alloc_pgtbl: out of memory!\n");
        return 0;
    }
    memset(pa, 0, PGSIZE);
    return (pgtbl_t)pa;
}

// 递归获取/创建PTE
pte_t* vm_getpte(pgtbl_t pgtbl, uint64 va, bool alloc)
{
    if (va >= MAXVA) {
        uart_puts("[vmem] vm_getpte: va out of range!\n");
        return 0;
    }

    pgtbl_t pt = pgtbl;
    for (int level = 2; level > 0; level--) {
        pte_t *pte = &pt[VA_TO_VPN(va, level)];
        if (*pte & PTE_V) {
            pt = (pgtbl_t)PTE_TO_PA(*pte);
        } else {
            if (!alloc) return 0;
            pgtbl_t new_pt = alloc_pgtbl();
            if (!new_pt) return 0;
            *pte = PA_TO_PTE(new_pt) | PTE_V;
            pt = new_pt;
        }
    }
    return &pt[VA_TO_VPN(va, 0)];
}

void vm_mappages(pgtbl_t pgtbl, uint64 va, uint64 pa, uint64 len, int perm)
{
    uint64 a = PGROUNDUP(va);
    uint64 last = PGROUNDUP(va + len - 1);

    for (; a <= last; a += PGSIZE, pa += PGSIZE) {
        pte_t *pte = vm_getpte(pgtbl, a, true);
        if (!pte) {
            uart_puts("[vmem] vm_mappages: no pte!\n");
            return;
        }
        if (*pte & PTE_V) {
            uart_puts("[vmem] vm_mappages: remap!\n");
            return;
        }
        *pte = PA_TO_PTE(pa) | perm | PTE_V;
    }
}

void vm_unmappages(pgtbl_t pgtbl, uint64 va, uint64 len, bool freeit)
{
    uint64 a = PGROUNDUP(va);
    uint64 last = PGROUNDUP(va + len - 1);

    for (; a <= last; a += PGSIZE) {
        pte_t *pte = vm_getpte(pgtbl, a, false);
        if (!pte || (*pte & PTE_V) == 0) {
            uart_puts("[vmem] vm_unmappages: not mapped!\n");
            return;
        }
        if (freeit) {
            uint64 pa = PTE_TO_PA(*pte);
            pmem_free(pa, true);
        }
        *pte = 0;
    }
}

// 打印页表（调试用）
void vm_print(pgtbl_t pgtbl)
{
    for (int i = 0; i < 512; i++) {
        pte_t pte = pgtbl[i];
        if (pte & PTE_V) {
            // 输出索引和对应的 PTE 值
            printf("[vmem] L2 entry: index=%d, PTE=0x%lx\n", i, pte);
        }
    }
}


// void kvm_init()
// {
//     kernel_pgtbl = alloc_pgtbl();
//     uart_puts("[vmem] kernel page table initialized\n");

//     // 1. 映射内核本身（恒等映射）
//     vm_mappages(kernel_pgtbl, KERNEL_BASE, KERNEL_BASE,
//                 PHYSTOP - KERNEL_BASE,
//                 PTE_R | PTE_W | PTE_X);

//     uart_puts("[vmem] kernel mapped\n");
// }

// void kvm_inithart()
// {
//     // 写 satp 寄存器，启用分页
//     w_satp(MAKE_SATP(kernel_pgtbl));
//     sfence_vma();  // 刷新TLB
//     uart_puts("[vmem] paging enabled on this hart\n");
// }

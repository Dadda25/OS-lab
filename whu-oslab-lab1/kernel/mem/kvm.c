#include "mem/kvm.h"
#include "mem/pmem.h"
#include "mem/vmem.h"
#include "lib/str.h"
#include "memlayout.h"
#include "dev/uart.h"
#include "lib/print.h"
#include <stdint.h>


// 内核页表
pgtbl_t kernel_pagetable;

// 外部符号
extern char etext[];       // 内核代码段结束
extern char trampoline[];  // trampoline 代码

pgtbl_t create_pagetable(void)
{
    pgtbl_t pagetable = (pgtbl_t)pmem_alloc(true);
    if (pagetable == NULL) {
        return NULL;
    }
    memset(pagetable, 0, PGSIZE);
    return pagetable;
}

void destroy_pagetable(pgtbl_t pagetable)
{
    for (int i = 0; i < 512; i++) {
        pte_t pte = pagetable[i];
        if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0) {
            // 下级页表
            uint64 child = PTE2PA(pte);
            destroy_pagetable((pgtbl_t)child);
            pagetable[i] = 0;
        }
    }
    pmem_free((uint64)pagetable, true);
}

pte_t* walk(pgtbl_t pagetable, uint64 va, int alloc)
{
    if (va >= MAXVA)
        return NULL;

    for (int level = 2; level > 0; level--) {
        pte_t *pte = &pagetable[PX(level, va)];
        if (*pte & PTE_V) {
            pagetable = (pgtbl_t)(uintptr_t)PTE2PA(*pte);
        } else {
            if (!alloc)
                return NULL;
            pgtbl_t new_table = (pgtbl_t)pmem_alloc(true);
            if (new_table == NULL)
                return NULL;
            memset(new_table, 0, PGSIZE);
            *pte = PA2PTE(new_table) | PTE_V;
            pagetable = new_table;
        }
    }
    return &pagetable[PX(0, va)];
}

int map_page(pgtbl_t pagetable, uint64 va, uint64 pa, int perm)
{
    pte_t *pte = walk(pagetable, va, 1);
    if (pte == NULL)
        return -1;
    if (*pte & PTE_V)
        return -1; // 已映射
    *pte = PA2PTE(pa) | perm | PTE_V;
    return 0;
}

int map_pages(pgtbl_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
    uint64 a, last;
    a = PGROUNDDOWN(va);
    last = PGROUNDDOWN(va + size - 1);
    for (;;) {
        if (map_page(pagetable, a, pa, perm) != 0)
            return -1;
        if (a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }
    return 0;
}

void unmap_pages(pgtbl_t pagetable, uint64 va, uint64 npages, int do_free)
{
    for (uint64 a = va; a < va + npages * PGSIZE; a += PGSIZE) {
        pte_t *pte = walk(pagetable, a, 0);
        if (pte == NULL || (*pte & PTE_V) == 0)
            continue;
        if (do_free) {
            uint64 pa = PTE2PA(*pte);
            pmem_free(pa, true);
        }
        *pte = 0;
    }
}

uint64 walkaddr(pgtbl_t pagetable, uint64 va)
{
    pte_t *pte = walk(pagetable, va, 0);
    if (pte == NULL)
        return 0;
    if ((*pte & PTE_V) == 0)
        return 0;
    if ((*pte & (PTE_R | PTE_X)) == 0)
        return 0;
    return PTE2PA(*pte);
}

void kvm_init(void)
{
    kernel_pagetable = create_pagetable();
    if (kernel_pagetable == NULL) {
        panic("kvminit: cannot alloc kernel pagetable");
    }

    // 映射内核代码段 [KERNELBASE, etext)，只读+可执行
    vm_mappages(kernel_pagetable, KERNEL_BASE, (uint64)etext - KERNEL_BASE,
              KERNEL_BASE, PTE_R | PTE_X);

    // 映射内核数据段 [etext, PHYSTOP)，读写
    map_pages(kernel_pagetable, (uint64)etext, PHYSTOP - (uint64)etext,
              (uint64)etext, PTE_R | PTE_W);

    // 映射UART寄存器
    map_pages(kernel_pagetable, UART0, PGSIZE, UART0, PTE_R | PTE_W);

    // 映射trampoline
    map_pages(kernel_pagetable, TRAMPOLINE, PGSIZE, (uint64)trampoline,
              PTE_R | PTE_X);
}

void kvm_inithart(void)
{
    // 切换到新的内核页表
    w_satp(MAKE_SATP(kernel_pagetable));
    sfence_vma();
}

void vmprint(pgtbl_t pagetable)

{
    // 简单调试输出
    for (int i = 0; i < 512; i++) {
        pte_t pte = pagetable[i];
        if (pte & PTE_V) {
            uint64 pa = PTE2PA(pte);
            printf("PTE[%d] = 0x%lx -> PA 0x%lx\n", i, pte, pa);
        }
    }
}

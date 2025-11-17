// #include "mem/kvm.h"
// #include "mem/pmem.h"
// #include "lib/str.h"
// #include "memlayout.h"
// #include "dev/uart.h"
// #include "lib/print.h"
// #include "riscv.h"


// // 全局内核页表（根页表）
// pgtbl_t kernel_pagetable = 0;

// // 外部符号（链接脚本或trampoline）
// extern char etext[];       // 代码段结束
// extern char trampoline[];  // trampoline 地址（如果有）

// // 创建一个新的空页表页（返回指向页表的虚拟地址）
// pgtbl_t create_pagetable(void)
// {
//     void *p = pmem_alloc(true); // 分配一个页面用作页表
//     if (!p) return NULL;
//     memset(p, 0, PGSIZE);
//     return (pgtbl_t)p;
// }

// // 递归销毁页表：释放所有低级页表页，然后释放本页
// void destroy_pagetable(pgtbl_t pagetable)
// {
//     if (!pagetable) return;

//     for (int i = 0; i < 512; i++) {
//         pte_t pte = pagetable[i];
//         if ((pte & PTE_V) && ((pte & (PTE_R|PTE_W|PTE_X)) == 0)) {
//             // 这是指向下级页表的PTE（非叶子）
//             uint64 child_pa = PTE2PA(pte);
//             pgtbl_t child = (pgtbl_t)child_pa;
//             // 注意：在某些内核实现中，需要把 PA 转换成内核可访问的 VA（+KERNEL_BASE）
//             destroy_pagetable(child);
//             pagetable[i] = 0;
//         } else {
//             // 叶子项或无效项，不在这里释放物理页（映射的物理页通常被其他机制管理）
//         }
//     }
//     // 释放当前页表页本身
//     pmem_free((uint64)pagetable, true);
// }

// // walk：返回指向 va 对应最低级 PTE 的指针（如果 alloc 为 1，允许在中间创建页表）
// pte_t* walk(pgtbl_t pagetable, uint64 va, int alloc)
// {
//     if (va >= MAXVA) return NULL; // 地址超出可支持范围

//     for (int level = 2; level > 0; level--) {
//         pte_t *pte = &pagetable[PX(level, va)];
//         if (*pte & PTE_V) {
//             // 有效项：如果是叶子（有 R/W/X），那这是映射冲突
//             if ((*pte & (PTE_R | PTE_W | PTE_X)) != 0) {
//                 // 已经是叶子（映射），不能当作页表
//                 return NULL;
//             }
//             // 指向下级页表
//             uint64 child_pa = PTE2PA(*pte);
//             pagetable = (pgtbl_t)child_pa;
//             // 如果内核地址空间在使用 KERNEL_BASE 偏移，可能需要转换：
//             // pagetable = (pgtbl_t)((uint64)child_pa + KERNEL_BASE);
//         } else {
//             if (!alloc) return NULL;
//             // 需要创建一个新的下级页表页
//             pgtbl_t new_table = create_pagetable();
//             if (!new_table) return NULL;
//             // 将新页表的物理地址写入PTE
//             *pte = PA2PTE((uint64)new_table) | PTE_V;
//             pagetable = new_table;
//         }
//     }
//     // 返回最终级别的 PTE
//     return &pagetable[PX(0, va)];
// }

// // 映射一段虚拟地址到物理地址（size 按字节），perm 为权限位（PTE_R|PTE_W|PTE_X|PTE_U...）
// int map_pages(pgtbl_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
// {
//     if (size == 0) return -1;

//     uint64 a = PGROUNDDOWN(va);
//     uint64 last = PGROUNDDOWN(va + size - 1);

//     for (; a <= last; a += PGSIZE, pa += PGSIZE) {
//         pte_t *pte = walk(pagetable, a, 1);
//         if (!pte) return -1;

//         if (*pte & PTE_V) {
//             // 可以选择覆盖或忽略，不再重复打印
//             //printf("map_pages: remap va 0x%lx (overwrite)\n", a);
//         }

//         *pte = PA2PTE(pa) | perm | PTE_V;
//     }
//     return 0;
// }

// void unmap_pages(pgtbl_t pagetable, uint64 va, uint64 npages, int do_free)
// {
//     uint64 first = PGROUNDDOWN(va);
//     uint64 last  = PGROUNDDOWN(va + npages * PGSIZE - 1);

//     for (uint64 a = first; a <= last; a += PGSIZE) {
//         pte_t *pte = walk(pagetable, a, 0);   // 第三个参数 0 表示不创建中间页表
//         if (!pte) continue;
//         if (*pte & PTE_V) {
//             uint64 pa = PTE2PA(*pte);        // 用你已有的 PTE2PA 宏
//             if (do_free) pmem_free(pa, 1);
//             *pte = 0;
//         }
//     }
// }

// void dump_pagetable(pgtbl_t pt, int level) {
//     // 递归打印页表，level 初始传 0
//     for (int i = 0; i < 512; i++) {
//         if (!(pt[i] & PTE_V))
//             continue;

//         uint64 pa = PTE2PA(pt[i]);
//         uint64 perm = pt[i] & 0xFFF;

//         for (int j = 0; j < level; j++)
//             uart_puts("  "); // 缩进显示层级
//         printf("[%d] VA level %d -> PA 0x%lx perm 0x%lx\n", i, level, pa, perm);

//         // 如果不是叶子页表，递归打印下一层
//         if (!(perm & PTE_R)) {
//             pgtbl_t next = (pgtbl_t)pa;
//             dump_pagetable(next, level + 1);
//         }
//     }
// }

// int map_region(pgtbl_t pt, uint64 va, uint64 pa, uint64 size, int perm) {
//     uint64 start = PGROUNDDOWN(va);
//     uint64 end   = PGROUNDDOWN(va + size - 1);
//     // 这里调用的map_pages()用来处理单页映射，map_region()是批量映射
//     for (uint64 a = start; a <= end; a += PGSIZE) {
//         if (map_pages(pt, a, PGSIZE, pa, perm) != 0) {
//             return -1; // 映射失败
//         }
//         pa += PGSIZE;
//     }
//     return 0; // 成功
// }

// // 激活页表，写入 satp 并刷新 TLB
// void kvminithart(void)
// {
//     uint64 satp_val = MAKE_SATP((uint64)kernel_pagetable);
//     w_satp(satp_val);
//     sfence_vma();
//     printf("kvminithart: paging enabled, satp = 0x%lx\n", satp_val);
// }

// // 内核页表初始化：创建 kernel_pagetable 并完成恒等映射（设备 + 可用内存）
// void kvminit(void)
// {
//     if (kernel_pagetable) return; // 已初始化

//     kernel_pagetable = create_pagetable();
//     if (!kernel_pagetable) {
//         panic("kvminit: cannot alloc kernel pagetable");
//     }

//     // 1) 映射内核代码段 [KERNEL_BASE, etext) 可执行+可读
//     uint64 code_size = (uint64)etext - KERNEL_BASE;
//     if (code_size > 0) {
//         map_pages(kernel_pagetable, KERNEL_BASE, KERNEL_BASE, code_size, PTE_R | PTE_X);
//     }

//     // 2) 映射内核数据段 [etext, PHYSTOP) 可读写
//     uint64 data_start = (uint64)etext;
//     uint64 data_size = PHYSTOP - data_start;
//     if (data_size > 0) {
//         map_pages(kernel_pagetable, data_start, data_start, data_size, PTE_R | PTE_W);
//     }

//     // 3) 映射常用设备 (UART、PLIC 等)
// #ifdef UART_BASE
//     map_pages(kernel_pagetable, UART_BASE, UART_BASE, PGSIZE, PTE_R | PTE_W);
// #endif

//     printf("kvminit: kernel pagetable created\n");
// }


// // 调试：打印页表（递归）
// // 这是一个简单的打印函数，便于调试页表条目
// void vmprint(pgtbl_t pagetable)
// {
//     printf("vmprint: pagetable @ %p\n", pagetable);
//     for (int level = 2; level >= 0; level--) {
//         for (int i = 0; i < 512; i++) {
//             pte_t pte = pagetable[i];
//             if ((pte & PTE_V) && ((pte & (PTE_R|PTE_W|PTE_X)) == 0)) {
//                 // 非叶子：下级页表
//                 uint64 child_pa = PTE2PA(pte);
//                 pgtbl_t child = (pgtbl_t)child_pa;
//                 // 可能需要在这里把物理 pa 转换为可访问的内核虚拟地址
//                 printf("  L%d idx %d: pte %lx -> child %p\n", level, i, pte, child);
//             } else if (pte & PTE_V) {
//                 uint64 pa = PTE2PA(pte);
//                 printf("  L%d idx %d: leaf pte %lx -> pa %lx\n", level, i, pte, pa);
//             }
//         }
//     }
// }

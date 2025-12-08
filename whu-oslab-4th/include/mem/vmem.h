/*
    虚拟内存管理 基于SV39

    satp寄存器: MODE(4) + ASID(16) + PPN(44)
    MODE控制虚拟内存模式 ASID与Flash刷新有关 PPN存放页表基地址

    基础页面大小为4KB
    VA:VPN[2] + VPN[1] + VPN[0] + offset      9 + 9 + 9 + 12 = 39  => 512GB
    PA:PPN[2] + PPN[1] + PPN[0] + offset     26 + 9 + 9 + 12 = 56
    
    PTE:Reserved PPN_2  PPN_1 + PPN_0 + RSW + D A G U X W R V
         10        26     9       9       2        1 * 8
    
    V : valid  
    R W X : read write execute 权限 (全0代表是非叶子节点,地址指向页表而非数据页) 
    U : 用户态是否能访问 (sstatus->SUM=1则内核态可以访问U=1的页)
    G : global mapping
    A D : accessed (是否访问过) dirty (是否写过) 配合页面置换算法
    RSW : 供S-mode自定义

    page_fault exception : 
    取指令时page不可执行 => fetch PF
    load或load_reserved时page不可读 => load PF
    store或store-conditional或AMP时page不可写 => write PF
*/
#ifndef __VMEM_H__
#define __VMEM_H__

#include "common.h"
#include "memlayout.h"

// page table entry   4KB / 8B = 512 entry
typedef uint64 pte_t;

// 页表基地址 pagetable可以理解为一个pte数组
typedef uint64* pgtbl_t;

// satp寄存器相关
#define SATP_SV39 (8L << 60)  // MODE = SV39
#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12)) // 设置MODE和PPN字段

// 获取虚拟地址中的虚拟页(VPN)信息 占9bit
#define VA_SHIFT(level)         (PAGE_OFFSET + 9 * (level))
#define VA_TO_VPN(va,level)     ((((uint64)(va)) >> VA_SHIFT(level)) & 0x1FF)

// PA和PTE之间的转换
#define PA_TO_PTE(pa) ((((uint64)(pa)) >> 12) << 10)
#define PTE_TO_PA(pte) (((pte) >> 10) << 12)

// 页面权限控制 
#define PTE_V (1 << 0)
#define PTE_R (1 << 1)
#define PTE_W (1 << 2)
#define PTE_X (1 << 3)
#define PTE_U (1 << 4)
#define PTE_G (1 << 5)
#define PTE_A (1 << 6)
#define PTE_D (1 << 7)
// 用户自定义
#define PTE_COW (1 << 8) // 写时复制
#define PTE_SHA (1 << 9) // 共享页面

#define PTE_FLAGS(pte) ((pte) & 0x3FF)  // 获取低10bit的flag信息


// mmap映射的region
typedef struct vm_region {
    uint64 start;           // 一个page的起始虚拟地址
    int npages;             // region 大小
    int flags;              // region flags
    struct vm_region* next; // 用于构造链表
} vm_region_t;

/*
    注意: 
    kvm开头函数的只有内核使用 
    uvm开头函数的只有用户使用 
    vm开头函数的两边都可以使用
    kvm 和 vm 实现于 kvm.c
    uvm 实现于 uvm.c
*/

// 初始化
void  uvm_init(void);
void  kvm_init(void);
void  kvm_inithart(void);

// 页表的申请 释放 复制

pgtbl_t uvm_alloc_pagetable(void);
void    uvm_free_pagetable(pgtbl_t pagetable);
void    uvm_free(pgtbl_t pagetable, uint64 sz, vm_region_t* vm_head); // 页表和data page都释放
int     uvm_copy_pagetable(pgtbl_t old, pgtbl_t new, uint64 sz, vm_region_t* head);

// 页面建立映射、解除映射、权限控制

int   vm_mappages(pgtbl_t pagetable, uint64 va, uint64 pa, uint64 len, int perm);
void  uvm_unmappages(pgtbl_t pagetable, uint64 va, uint64 npages, bool freeit);
uint64 uvm_protect(uint64 start, int len, int prot);

// 内核与用户的数据传递

int  vm_copyout(bool usermode, uint64 dst, void* src, uint64 len);
int  vm_copyin(bool usermode, void* dst, uint64 src, uint64 len);
int  uvm_copyout(pgtbl_t pagetable, uint64 dst, uint64 src, uint64 len);
int  uvm_copyin(pgtbl_t pagetable, uint64 dst, uint64 src, uint64 len);
int  uvm_copyin2(pgtbl_t pagetable, char* dst, uint64 srcva, uint64 maxlen);

// 用户地址空间的扩大与缩小

uint64  uvm_grow(pgtbl_t pagetable, uint64 oldsz, uint64 newsz, int xperm);
uint64  uvm_ungrow(pgtbl_t pagetable, uint64 oldsz, uint64 newsz);
uint64  uvm_mmap(uint64 start, int len, int prot, int flags, int fd, int off);
uint64  uvm_munmap(uint64 start, int len);

// 虚拟内存模块的辅助函数

pte_t*       vm_getpte(pgtbl_t pagetable, uint64 va, bool alloc);
uint64       uvm_getpa(pgtbl_t pagetable, uint64 va);
vm_region_t* uvm_region_alloc();
void         uvm_region_free(pgtbl_t pagetable ,vm_region_t* region);

// 其他特殊函数:
// 在建立第一个进程时负责映射initcode.S
// 在exec.c中负责建立用户屏障

void  uvm_map_initcode(pgtbl_t pagetable, unsigned char* src, uint32 sz);
void  uvm_clear_PTEU(pgtbl_t pagetable, uint64 va);


// mmap->prot
#define PROT_NONE      0x0
#define PROT_READ      0x1
#define PROT_WRITE     0x2
#define PROT_EXEC      0x4
// #define PROT_GROWSDOWN 0X01000000
// #define PROT_GROWSUP   0X02000000

// mmap->flags
#define MAP_FILE       0x00
#define MAP_SHARED     0x01       // 共享的（与其他进程）
#define MAP_PRIVATE    0X02       // 私有的
#define MAP_FIXED      0x10
#define MAP_ANONYMOUS  0x20
#define MAP_FAILED     ((void *) -1)

#endif
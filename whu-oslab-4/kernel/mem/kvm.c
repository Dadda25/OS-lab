#include "lib/str.h"
#include "lib/print.h"
#include "mem/pmem.h"
#include "mem/vmem.h"
#include "proc/cpu.h"
#include "memlayout.h"
#include "riscv.h"

pgtbl_t kernel_pagetable; // 指向内核页表

extern char trampoline[]; // in trampoline.S 

// 申请内核页表并完成一些基本映射
void kvm_init(void)
{
    // 申请L2内核页表空间
    kernel_pagetable = (pgtbl_t)pmem_alloc_pages(1, true);
    // printf("kernel pagetable = %p\n",kernel_pagetable);
    assert(kernel_pagetable != NULL, "kvm.c->kvm_init: 1\n");
    memset(kernel_pagetable, 0, PAGE_SIZE);

    int ret = 0;
    // uart寄存器映射
    ret += vm_mappages(kernel_pagetable, UART_BASE, UART_BASE, PAGE_SIZE, PTE_R | PTE_W);
    // virtio寄存器映射
    ret += vm_mappages(kernel_pagetable, VIO_BASE, VIO_BASE, PAGE_SIZE, PTE_R | PTE_W);
    // PLIC映射
    ret += vm_mappages(kernel_pagetable, PLIC_BASE, PLIC_BASE, 0x400000, PTE_R | PTE_W);
    // RTC实时时钟映射
    ret += vm_mappages(kernel_pagetable, RTC_BASE, RTC_BASE, PAGE_SIZE, PTE_R | PTE_W);
    // kernel代码区映射
    ret += vm_mappages(kernel_pagetable, KERNEL_BASE, KERNEL_BASE, KERNEL_TEXT-KERNEL_BASE, PTE_R | PTE_X);
    // kernel数据区映射
    ret += vm_mappages(kernel_pagetable, KERNEL_TEXT, KERNEL_TEXT, USER_END-KERNEL_TEXT, PTE_R | PTE_W);
    // trampoline映射
    ret += vm_mappages(kernel_pagetable, TRAMPOLINE, (uint64)trampoline, PAGE_SIZE, PTE_R | PTE_X);
    
    // 验证映射是否成功
    assert(ret == 0,"vmem.c->kvm_init 2\n");
    
    // 进程列表的内核栈映射
    proc_mapstacks(kernel_pagetable);
}

// 开启分页模式
void kvm_inithart(void)
{
    // flush the TLB
    sfence_vma();
    // 写入内核页表,开启sv39模式的分页
    w_satp(MAKE_SATP(kernel_pagetable));
    // flush the TLB
    sfence_vma();
}


// 根据pagetable,找到va对应的pte
// 若设置alloc=true 则在PTE无效时尝试申请一个物理页
// 成功返回PTE, 失败返回NULL
pte_t* vm_getpte(pgtbl_t pagetable, uint64 va, bool alloc)
{
    assert(va < VA_MAX, "kvm.c->vm_getpte\n");
    
    for(int level = 2; level > 0; level--) {

        // 在当前页表下,找到va对应的pte
        pte_t* pte = &pagetable[VA_TO_VPN(va, level)]; 
        
        if(*pte & PTE_V) {   // 有效PTE

            // 更新pagetable指向下一级页表
            pagetable = (pgtbl_t)PTE_TO_PA(*pte);
        
        } else if(alloc) {  // 无效PTE但是尝试申请
        
            // 申请一个物理页作为页表并清空
            pagetable = (pgtbl_t)pmem_alloc_pages(1, true);
            if(pagetable == NULL) return NULL;
            memset(pagetable, 0, PAGE_SIZE);

            // 修改PTE中的物理地址并设为有效
            *pte = PA_TO_PTE(pagetable) | PTE_V;
        
        } else {           // 无效且不尝试申请
        
            return NULL;
        
        }
    }

    return &pagetable[VA_TO_VPN(va, 0)];
}

 
// 在pagetable中建立映射 [va, va+len)->[pa, pa+len) 
// 映射是页面级的, 页面权限为perm
// pa 应当保证page-aligned, va 和 len 不需要保证
// 成功返回0, 失败返回-1
int vm_mappages(pgtbl_t pagetable, uint64 va, uint64 pa, uint64 len, int perm)
{
    assert(pa % PAGE_SIZE == 0,"veme.c->vm_mappages\n");

    // 确定映射范围
    uint64 first_page = ALIGN_DOWN(va, PAGE_SIZE);
    uint64 last_page  = ALIGN_DOWN(va+len-1, PAGE_SIZE);
    uint64 cur_page   = first_page;
    
    pte_t* pte;
    int count = 0;
    // 开始逐页映射
    
    while(cur_page <= last_page) {
        
        // 拿到pte并修改它
        pte = vm_getpte(pagetable, cur_page, true);
        if(pte == NULL) goto fail;
        *pte = PA_TO_PTE(pa) | perm | PTE_V;

        // 迭代
        cur_page += PAGE_SIZE;
        pa       += PAGE_SIZE;
        count++; 
    }

    return 0;

fail:
    uvm_unmappages(pagetable, first_page, count, false);
    return -1;
}


// 数据复制: src->dst (len bytes)
// 如果目的地址是用户空间,执行uvm_copyout,否则执行memmove
// 成功返回0, 失败返回-1
int vm_copyout(bool usermode, uint64 dst, void* src, uint64 len)
{
    proc_t* proc = myproc();
    if(usermode) {
        return uvm_copyout(proc->pagetable, dst, (uint64)src, len);
    } else {
        memmove((char*)dst, src, len);
        return 0;
    }
}

// 数据复制: src->dst (len bytes)
// 如果源地址是用户空间,执行uvm_copyin,否则执行memmove
// 成功返回0, 失败返回-1
int vm_copyin(bool usermode, void* dst, uint64 src, uint64 len)
{
    proc_t* proc = myproc();
    if(usermode) {
        return uvm_copyin(proc->pagetable, (uint64)dst, src, len);
    } else {
        memmove(dst, (char*)src, len);
        return 0;
    }
}
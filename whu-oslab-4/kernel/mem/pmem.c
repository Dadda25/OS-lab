/* 物理内存管理 */

#include "lib/print.h"
#include "lib/str.h"
#include "mem/pmem.h"
#include "lock/lock.h"
#include "memlayout.h"

/*
--------------------------------- USER_END    (0x8800-0000)

// 可分配的用户数据区

--------------------------------- USER_BASE   (4KB对齐)

// 可分配的内核数据区 
(size = KERNEL_PAGE_NUM * 4KB)

--------------------------------- KERNEL_DATA (4KB对齐)

// 内核静态数据 (这里面包括了内核栈,在start.S中定义)

--------------------------------- KERNEL_TEXT (4KB对齐)

// 内核代码

--------------------------------- KERNEL_BASE (0x8000-2000)

// open-sbi的代码和数据

--------------------------------- Open_SBI    (0x8000-0000)

// 内存映射区域

---------------------------------
*/

uint64 USER_BASE;

#define KERNEL_PAGE_NUM 1024 // 4MB 内核空间

void* vio_mem; // 两个连续物理页，在virtio中使用

typedef struct listnode {
    struct listnode* next;
} listnode_t;

// 内核空闲页链表
struct {
    listnode_t freelist;
    spinlock_t lk;
} kmem;

// 用户空闲页链表
struct {
    listnode_t freelist;
    spinlock_t lk;
} umem;


// 物理内存初始化
void pmem_init(bool output)
{
    uint64 pa;
    listnode_t* node;

    spinlock_init(&kmem.lk, "kern mem");
    spinlock_init(&umem.lk, "user mem");
    kmem.freelist.next = NULL;
    umem.freelist.next = NULL;

    USER_BASE = KERNEL_TEXT + KERNEL_PAGE_NUM * PAGE_SIZE;
    
    // 内核空间 页链表的建立    
    spinlock_acquire(&kmem.lk);
    for(pa = KERNEL_DATA; pa < USER_BASE; pa += PAGE_SIZE) {
        node = (listnode_t*)pa;
        node->next = kmem.freelist.next;
        kmem.freelist.next = node;
    }
    spinlock_release(&kmem.lk);

    // 用户空间 页链表的建立
    spinlock_acquire(&umem.lk);
    for(pa = USER_BASE; pa < USER_END; pa += PAGE_SIZE) {
        node = (listnode_t*)pa;
        node->next = umem.freelist.next;
        umem.freelist.next = node;        
    }
    spinlock_release(&umem.lk);

    vio_mem = pmem_alloc_pages(1, true);
    pmem_alloc_pages(1, true);

    if(output) {
        printf("here is memlayout:\n");
        printf("kern_base = 0x0000-0000-8020-0000\n");
        printf("kern_text = %p\n",KERNEL_TEXT);
        printf("kern_data = %p\n",KERNEL_DATA);
        printf("user_base = %p\n",USER_BASE);
        printf("user_end  = 0x0000-0000-8800-0000\n");
        printf("pmem_init success!\n");
    }
}

/*
    申请npage个4K物理页
    成功返回物理页地址 失败返回NULL
*/
void* pmem_alloc_pages(int npages, bool in_kernel)
{
    assert(npages == 1, "pmem_alloc_pages\n");
    
    listnode_t* node;

    if(in_kernel) {
        spinlock_acquire(&kmem.lk);
        node = kmem.freelist.next;
        if(node) kmem.freelist.next = node->next;
        else kmem.freelist.next = NULL;
        spinlock_release(&kmem.lk);
    } else {
        spinlock_acquire(&umem.lk);
        node = umem.freelist.next;
        if(node) umem.freelist.next = node->next;
        else umem.freelist.next = NULL;
        spinlock_release(&umem.lk);
    }

    return (void*)node;
}
/*
    释放npages个物理页,从ptr指向的地址开始
*/
void pmem_free_pages(void* ptr, int npages, bool in_kernel)
{     
    assert(npages == 1, "pmem_free_pages: 1\n");
    assert((uint64)ptr % PAGE_SIZE == 0, "pmem_free_pages: 2\n");

    memset(ptr, 0, PAGE_SIZE);
    listnode_t* node = (listnode_t*)ptr;

    if(in_kernel) {
        
        assert((uint64)ptr >= KERNEL_DATA && (uint64)ptr < USER_BASE, "pmem_free_pages: 3\n");
        
        spinlock_acquire(&kmem.lk);
        node->next = kmem.freelist.next;
        kmem.freelist.next = node;
        spinlock_release(&kmem.lk);

    } else {
        
        assert((uint64)ptr >= USER_BASE && (uint64)ptr < USER_END, "pmem_free_pages: 4\n");
        
        spinlock_acquire(&umem.lk);
        node->next = umem.freelist.next;
        umem.freelist.next = node;
        spinlock_release(&umem.lk);
    
    }
}
#include "mem/pmem.h"
#include "lib/str.h"
#include "memlayout.h"
#include "dev/uart.h"

struct {
    struct run *freelist;
    uint64 total_pages;
    uint64 free_pages;
} pmem;

// 外部符号，由链接脚本提供
extern char end[];

// 内部函数
static void free_one_page(void *pa);

void pmem_init(void)
{
    char *mem_start = (char*)PGROUNDUP((uint64)end);
    char *mem_end   = (char*)PHYSTOP;

    uart_puts("[pmem] initializing physical memory manager...\n");

    pmem.freelist = NULL;
    pmem.total_pages = (PHYSTOP - (uint64)mem_start) / PGSIZE;
    pmem.free_pages  = 0;

    for (char *p = mem_end - PGSIZE; p >= mem_start; p -= PGSIZE) {
        free_one_page(p);
        if (p == mem_start) break; // 避免无符号溢出
    }


    uart_puts("[pmem] init done. total pages=");
    // 简单输出数字，调试用
    // 建议你实现 uart_putd() 打印整数
    uart_puts(" (set uart_putd to see)\n");
}

void* pmem_alloc(bool in_kernel)
{
    struct run *r;

    if (pmem.freelist == NULL) return NULL;

    r = pmem.freelist;
    pmem.freelist = r->next;
    pmem.free_pages--;

    memset((char*)r, 0, PGSIZE);
    return (void*)r;
}

void pmem_free(uint64 pa, bool in_kernel)
{
    free_one_page((void*)pa);
}

void* pmem_alloc_pages(int n, bool in_kernel)
{
    if (n <= 0) return NULL;
    if (n == 1) return pmem_alloc(in_kernel);

    // 简单实现：循环分配
    char *first = pmem_alloc(in_kernel);
    if (!first) return NULL;

    for (int i = 1; i < n; i++) {
        if (!pmem_alloc(in_kernel)) {
            // 回收已分配的
            for (int j = 0; j < i; j++) {
                pmem_free((uint64)(first + j * PGSIZE), in_kernel);
            }
            return NULL;
        }
    }
    return first;
}

int pmem_free_pages_count(void)
{
    return pmem.free_pages;
}

void pmem_get_stat(struct pmem_stat *stat)
{
    stat->total_pages = pmem.total_pages;
    stat->free_pages  = pmem.free_pages;
    stat->used_pages  = pmem.total_pages - pmem.free_pages;
}

// 内部辅助函数
static void free_one_page(void *pa)
{
    if (((uint64)pa % PGSIZE) != 0) {
        uart_puts("[pmem] free_one_page: address not aligned!\n");
        return;
    }

    if ((char*)pa < end || (uint64)pa >= PHYSTOP) {
        uart_puts("[pmem] free_one_page: address out of range!\n");
        return;
    }

    memset(pa, 1, PGSIZE); // 方便调试
    struct run *r = (struct run*)pa;
    r->next = pmem.freelist;
    pmem.freelist = r;
    pmem.free_pages++;
}

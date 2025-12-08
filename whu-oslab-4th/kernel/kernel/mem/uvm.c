#include "mem/vmem.h"
#include "mem/pmem.h"
#include "proc/cpu.h"
#include "lib/print.h"
#include "lib/str.h"
#include "memlayout.h"
#include "fs/ext4_file.h"
#include "fs/ext4_inode.h"
#include "proc/proc.h"
#include "common.h"

#define N_VM_REGION 128

// vm_region 数组
static vm_region_t vm_region_list[N_VM_REGION];

// vm_region_list init
void uvm_init()
{
    for(int i = 0; i < N_VM_REGION; i++) {
        vm_region_list[i].start = 0;
        vm_region_list[i].npages = 0;
        vm_region_list[i].flags = 0;
        vm_region_list[i].next = NULL;
    }
}

// 返回一个可以用的vm_region
vm_region_t* uvm_region_alloc()
{
    vm_region_t* region = NULL;
    for(int i = 0; i < N_VM_REGION; i++) {
        if(vm_region_list[i].start == 0) {
            region = &vm_region_list[i];
            break;
        }
    }
    assert(region != NULL, "vm_region_alloc");
    return region;
}

// 释放region资源, 同时释放占用的物理页
void uvm_region_free(pgtbl_t pagetable, vm_region_t* region)
{
    uvm_unmappages(pagetable, region->start, region->npages, true);
    region->start = 0;
    region->npages = 0;
    region->next = NULL;
    region->flags = 0;
}

//  逐级查询pagetable找到va对应的pa
//  成功返回pa,若失败则返回0
uint64 uvm_getpa(pgtbl_t pagetable, uint64 va)
{
    assert(va < VA_MAX, "uvm_getpa");
    pte_t* pte = vm_getpte(pagetable, va, false);
    // 确认拿到的PTE是有效的用户态的PTE
    if(pte == NULL) return 0;
    if((*pte & PTE_V) == 0) return 0;
    if((*pte & PTE_U) == 0) return 0;
    uint64 pa = PTE_TO_PA(*pte);
    return pa;
}

//  对从va开始的npages个页面解除映射
//  va需保证page-aligned
//  如果freeit置为true,同时释放被映射的物理页
void uvm_unmappages(pgtbl_t pagetable, uint64 va, uint64 npages, bool freeit)
{
    assert(va % PAGE_SIZE == 0, "uvm_unmappages 1\n");
    pte_t* pte;

    // printf("va = %p npages = %d\n", va, npages);
    for(uint64 cur_va = va; cur_va < va + PAGE_SIZE * npages; cur_va += PAGE_SIZE) {
        
        // 获取 pte 并验证 非空 + 有效 + 指向data page
        pte = vm_getpte(pagetable, cur_va, false);
        // printf("va = %p, pte = %p\n", cur_va, *pte);
        assert(pte != NULL, "uvm_unmappages 2\n");
        assert((*pte) & PTE_V, "uvm_unmappages 3\n");
        assert((*pte) & (PTE_R | PTE_W | PTE_X) , "uvm_unmappages 4\n");
        
        // 释放占用的物理页
        if(freeit) pmem_free_pages((void*)PTE_TO_PA(*pte), 1, false);
        
        // 清空pte
        *pte = 0;
    }
}

//  申请一个L2用户态页表并清空
//  成功返回pagetable, 失败返回NULL
pgtbl_t uvm_alloc_pagetable() 
{
    pgtbl_t pagetable = (pgtbl_t)pmem_alloc_pages(1, true);
    if(pagetable != NULL) 
        memset(pagetable, 0, PAGE_SIZE);
    return pagetable;
}

//  解除L2->L1,L1->L0这两级pagetable的映射关系
//  同时回收pagetable占用的物理页
//  (通常在释放页表管理的数据区域后使用)
void uvm_free_pagetable(pgtbl_t pagetable)
{
    pte_t pte;
    uint64 child;

    // 遍历整个页表，找到有效PTE并递归清除
    for(int i = 0; i < PAGE_SIZE / sizeof(pte); i++) {
        pte = pagetable[i];
        if(pte & PTE_V) { 
            // 它不指向data page而是下一级页表
            if ((pte & (PTE_R | PTE_W | PTE_X)) == 0) {
                child = PTE_TO_PA(pte);             // 找到下一级页表
                uvm_free_pagetable((pgtbl_t)child); // 递归清除
                pagetable[i] = 0;                   // 清空此PTE 
            } else {
                printf("pte = %p\n", pte);
                panic("kvm.c->uvm_free_pagetable");
            }
        }
    }
    // 回收页表占用的物理页
    pmem_free_pages((void*)pagetable, 1, true);
}

//  拷贝页表和它管理的物理页(old->new sz字节)
//  成功返回0,失败返回-1
int uvm_copy_pagetable(pgtbl_t old, pgtbl_t new, uint64 sz, vm_region_t* head)
{
    uint64 pa, va;
    pte_t* pte;
    int flags, ret;
    char* mem;

    // [0, sz]区域复制
    for(va = 0; va < sz; va += PAGE_SIZE) {

        // 获得pte并进行检查
        pte = vm_getpte(old, va, false);
        assert(pte != NULL, "uvm_copy_pagetable: 1\n");
        assert((*pte) & PTE_V, "uvm_copy_pagetable: 2\n");
        assert((*pte) & (PTE_R | PTE_W | PTE_X), "uvm_copy_pagetable: 3\n");

        // pte -> pa + flags
        pa    = PTE_TO_PA(*pte);
        flags = PTE_FLAGS(*pte);

        // 申请一个新物理页并使用data page填充
        mem = (char*)pmem_alloc_pages(1, false);
        if(mem == NULL) goto fail;
        memmove(mem, (const void*)pa, PAGE_SIZE);
        
        // 尝试映射到新的pagetable
        ret = vm_mappages(new, va, (uint64)mem, PAGE_SIZE, flags);
        if(ret < 0) {
            pmem_free_pages(mem, 1, false);
            goto fail;
        }
    }

    // vm_region区域复制
    vm_region_t* tmp = head;
    while(tmp != NULL) {
        for(int i = 0; i < tmp->npages; i++) {
            va = tmp->start + i * PAGE_SIZE;
            pte = vm_getpte(old, va, false);
            assert(pte != NULL, "uvm_copy_pagetable: 4\n");
            assert((*pte) & PTE_V, "uvm_copy_pagetable: 5\n");
            assert((*pte) & (PTE_R | PTE_W | PTE_X), "uvm_copy_pagetable: 6\n");
            
            pa    = PTE_TO_PA(*pte);
            flags = PTE_FLAGS(*pte);
            mem = pmem_alloc_pages(1, false);
            assert(mem != NULL, "uvm_copy_pagetable: 7");
            memmove(mem, (const void*)pa, PAGE_SIZE);
            assert(vm_mappages(new, va, (uint64)mem, PAGE_SIZE, flags) == 0, "uvm_copy_pagetable: 8");
        }
        tmp = tmp->next;
    }

    return 0;

fail:
    // 解除所有映射并释放对应物理页
    uvm_unmappages(new, 0, va / PAGE_SIZE, true);
    return -1;
}

//  解除进程[0,sz)的地址映射并释放data pages
//  然后销毁页表 (释放地址空间)
void uvm_free(pgtbl_t pagetable, uint64 sz, vm_region_t* vm_head)
{
    vm_region_t *tmp = vm_head, *next = NULL;
    
    // 解除映射并释放物理页
    if(sz != 0) 
        uvm_unmappages(pagetable, 0, ALIGN_UP(sz, PAGE_SIZE) / PAGE_SIZE, true);
    while (tmp) {
        next = tmp->next;
        uvm_region_free(pagetable, tmp);
        tmp = next;
    }

    // 销毁页表
    uvm_free_pagetable(pagetable);
}

//  把initcode的可执行文件src载入用户页表0地址处
//  (第一个进程创建时使用)
void uvm_map_initcode(pgtbl_t pagetable, unsigned char* src, uint32 sz)
{
    char* mem;
    uint32 begin, cut_len, left_len = sz;

    for(begin = 0; begin < sz; begin += PAGE_SIZE) {
        // 申请一个用户地址空间的物理页
        mem = (char*)pmem_alloc_pages(1, false);
        assert(mem != NULL, "uvm_map_initcode: 2\n"); 
        
        // 数据转移
        cut_len = min(PAGE_SIZE, left_len);
        memmove(mem, src, cut_len);
        src += cut_len;

        // 作为可读可写可执行的用户页面映射至虚拟地址begin处
        assert(vm_mappages(pagetable, begin, (uint64)mem, PAGE_SIZE, PTE_R|PTE_W|PTE_X|PTE_U) == 0,
        "uvm_map_initcode: 3\n");
    }
    // 栈空间
    mem = (char*)pmem_alloc_pages(1, false);
    assert(mem != NULL, "uvm_map_initcode: 4\n"); 
    vm_mappages(pagetable, begin, (uint64)mem, PAGE_SIZE, PTE_R|PTE_W|PTE_U);
}

//  找到va对应的PTE,将PTE_U置为0,使得这个页面在U-mode不可访问
//  在exec.c->main中用于建立屏障
void uvm_clear_PTEU(pgtbl_t pagetable, uint64 va)
{
    pte_t* pte = vm_getpte(pagetable, va, false);
    assert(pte != NULL, "uvm.c->uvm_clear_PTEU\n");
    *pte = *pte & (~PTE_U);
}


//  申请新的物理页并加入页表,权限位设为xperm
//  使得进程控制的物理内存范围从oldsz增长到newsz
//  成功返回newsz,失败返回0或oldsz
uint64 uvm_grow(pgtbl_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
    // 正常情况下 newsz > oldsz
    if(newsz <= oldsz) return oldsz;
    
    char* mem;
    oldsz = ALIGN_UP(oldsz, PAGE_SIZE);
    for(uint64 cur_page = oldsz; cur_page < newsz; cur_page += PAGE_SIZE) {
        // 申请物理页,失败则撤回前面的工作
        mem = pmem_alloc_pages(1, false);
        if(mem == NULL) {
            uvm_ungrow(pagetable, cur_page, oldsz);
            return 0;
        }
        memset(mem, 0, PAGE_SIZE);
        // 页表映射,失败则撤回前面的工作
        if(vm_mappages(pagetable, cur_page, (uint64)mem, PAGE_SIZE, PTE_U | xperm) < 0) {
            pmem_free_pages(mem, 1, false); // 其他alloc的物理页由uvm_ungrow负责释放
            uvm_ungrow(pagetable, cur_page, oldsz);
            return 0;
        }
    }
    return newsz;
}

//  基于页表解除一部分物理页的映射并释放它们
//  使得进程控制的物理内存大小从oldsz缩减到newsz
//  成功返回newsz, 失败返回oldsz
uint64 uvm_ungrow(pgtbl_t pagetable, uint64 oldsz, uint64 newsz)
{
    // 正常情况: newsz < oldsz
    if(newsz >= oldsz) return oldsz;

    uint64 round_oldsz = ALIGN_UP(oldsz, PAGE_SIZE);
    uint64 round_newsz = ALIGN_UP(newsz, PAGE_SIZE); 
    if( round_newsz < round_oldsz) {
        int npages = (round_oldsz - round_newsz) / PAGE_SIZE;
        uvm_unmappages(pagetable, round_newsz, npages, true);
    }
    return newsz;
}

//  linux页面权限 => PTE的页面权限
static int prot_to_xperm(int flags)
{
    int xperm = 0;
    if(flags & 1) xperm |= PTE_R;
    if(flags & 2) xperm |= PTE_W;
    if(flags & 4) xperm |= PTE_X;
    return xperm;
}

//  进程数据空间的扩展与映射
//  dst应当为进程的sz 扩展到 sz + len
//  [src, src+len) -> [dst, dst+len) 读写权限为flags
//  成功返回映射区域指针 失败返回-1
uint64 uvm_map(pgtbl_t pagetable, uint64 dst, int len, uint64 src, int prot)
{
    int xperm = prot_to_xperm(prot);
    
    uint64 newsz = uvm_grow(pagetable, dst, dst + len, xperm);
    if(newsz != dst + len) return -1;

    int ret  = uvm_copyout(pagetable, dst, src, len);
    if(ret < 0) return -1;
    
    return dst;
}

//  进程数据空间的缩小与解映射
//  成功返回0 失败返回-1
int uvm_ummap(pgtbl_t pagetable, uint64 dst, int len)
{
    uint64 newsz = uvm_ungrow(pagetable, dst + len, dst);
    if(newsz != dst) return -1;
    else return 0;
}

//  从src指向的内核地址中 复制len字节 到dstva指向的用户地址 (kernel->user)
//  成功返回0, 失败返回-1
int uvm_copyout(pgtbl_t pagetable, uint64 dst, uint64 src, uint64 len)
{
    uint64 va0, pa0, n;

    while (len > 0) {
        // 虚拟地址->物理地址
        va0 = ALIGN_DOWN(dst, PAGE_SIZE);
        pa0 = uvm_getpa(pagetable, va0);
        if(pa0 == 0) return -1;
        // 确认本次迁移的长度
        n = PAGE_SIZE-(dst-va0); // 取上半部分
        if(n > len) n = len;       // 取下半部分
        // 数据迁移
        memmove((void *)(pa0+(dst-va0)), (void*)src, n);
        // 迭代
        src += n;
        dst += n;
        len -= n;
    }
    return 0;
}
 
//  从srcva指向的虚拟地址中 复制len字节 到dst指向的物理地址 (user->kernel)
//  成功返回0, 失败返回-1
int uvm_copyin(pgtbl_t pagetable, uint64 dst, uint64 srcva, uint64 len)
{
    uint64 va0, pa0, n;

    while (len > 0) {
        // 虚拟地址->物理地址
        va0 = ALIGN_DOWN(srcva, PAGE_SIZE);
        pa0 = uvm_getpa(pagetable,va0);
        if(pa0 == 0) return -1;
        // 确认本次迁移的长度
        n = PAGE_SIZE - (srcva - va0);
        if(n > len) n = len;
        // 数据迁移
        memmove((void*)dst, (void*)(pa0 + (srcva - va0)), n);
        // 迭代
        dst += n;
        srcva += n;
        len -= n;
    }
    return 0;
}

//  思路和uvm_copyin一样,但不知道待处理的内存区域具体长度
//  已知这块区域是字符串且长度不超过maxlen,将检查'\0'这个终结符
int uvm_copyin2(pgtbl_t pagetable, char* dst, uint64 srcva, uint64 maxlen)
{
    uint64 len, va0, pa0;
    bool get_null = false; // 是否遇到'\0'
    char* p;

    while(!get_null && maxlen > 0) {
    
        va0 = ALIGN_DOWN(srcva, PAGE_SIZE);
        pa0 = uvm_getpa(pagetable, va0);
        if(pa0 == 0) return -1;

        // 获得待处理字符串长度
        len = PAGE_SIZE - (srcva - va0);
        if(len > maxlen) len = maxlen;

        // p指向待处理字符串
        p = (char*)(pa0 +(srcva - va0));

        for(int i=0; i<len; i++) {
            dst[i] = p[i];
            if(p[i] == '\0') {
                get_null = true;
                break;
            }
        }
        dst += len;
        srcva += len;
        maxlen -= len;
    }
    return get_null ? 0 : -1;
}

// sys_mmap的实现
// 成功返回已映射区域的指针, 失败返回-1
uint64 uvm_mmap(uint64 start, int len, int prot, int flags, int fd, int off)
{
    // printf("uvm_mmap: start = %p, len = %d, prot = %d, flags = %d, fd = %d, off = %d\n", 
    //        start, len, prot, flags, fd, off);
    proc_t* p = myproc();
    vm_region_t* vm_region = uvm_region_alloc();
    int perm = PTE_U;
    uint64 pa, va, ret = p->vm_allocable;

    if(prot == PROT_NONE) return -1;
    // assert(prot != PROT_NONE, "uvm_mmap: -1");

    if(prot & PROT_READ)
        perm |= PTE_R;
    if(prot & PROT_WRITE)
        perm |= PTE_W;
    if(prot & PROT_EXEC)
        perm |= PTE_X;

    //printf("uvm_mmap: start = %p, len = %d, prot = %d, flags = %d, fd = %d, off = %d\n", start, len, prot, flags, fd, off);
    
    assert(start == 0, "uvm_mmap: 0");
    //assert(flags & MAP_SHARED, "uvm_mmap: 1");
    
    // 确保至少映射一个页面，即使长度为0
    if(len <= 0) len = PAGE_SIZE;
    len = ALIGN_UP(len, PAGE_SIZE) / PAGE_SIZE;
    // printf("%d\n", len);

    spinlock_acquire(&p->lk);
    if(flags & MAP_ANONYMOUS) { // 匿名映射

        vm_region->start = p->vm_allocable;
        vm_region->npages = len;
        vm_region->flags = flags;
        if(p->vm_head) {
            vm_region->next = p->vm_head->next;
            p->vm_head->next = vm_region;
        } else {
            vm_region->next = NULL;
            p->vm_head = vm_region;
        }

        for(int i = 0; i < len; i++) {
            pa = (uint64)pmem_alloc_pages(1, false);
            if(pa == 0) goto fail;
            va = p->vm_allocable;
            p->vm_allocable += PAGE_SIZE;
            if(vm_mappages(p->pagetable, va, pa, PAGE_SIZE, perm) < 0) goto fail;
        }
    } else {                    // 文件映射
        // 验证文件描述符
        if(fd < 0 || fd >= NOFILE || p->ext4_ofile[fd] == NULL) {
            // printf("uvm_mmap: invalid file descriptor fd=%d (NOFILE=%d, ofile=%p)\n", 
            //        fd, NOFILE, fd >= 0 && fd < NOFILE ? p->ext4_ofile[fd] : NULL);
            goto fail;
        }
        
        ext4_file_t* file = p->ext4_ofile[fd];
        //printf("uvm_mmap: file mapping, fd=%d, file->ip->size=%d\n", fd, file->ip->size);
        
        // 使用现有的vm_region分配函数
        vm_region->start = p->vm_allocable;
        vm_region->npages = len;
        vm_region->flags = flags;
        
        // 添加到进程的vm_region链表
        if(p->vm_head != NULL) {
            vm_region->next = p->vm_head->next;
            p->vm_head->next = vm_region;
        } else {
            vm_region->next = NULL;
            p->vm_head = vm_region;
        }

        // 为每个页面分配物理内存并读取文件内容
        for(int i = 0; i < len; i++) {
            pa = (uint64)pmem_alloc_pages(1, false);
            if(pa == 0) goto fail;
            
            va = p->vm_allocable;
            p->vm_allocable += PAGE_SIZE;
            
            // 从文件读取数据到物理页面
            uint64 file_offset = off + i * PAGE_SIZE;
            uint32 read_size = PAGE_SIZE;
            
            // 如果是文件末尾，可能读取不足一页的数据
            if(file_offset + PAGE_SIZE > file->ip->size) {
                if(file_offset >= file->ip->size) {
                    read_size = 0;  // 超出文件大小，填零
                } else {
                    read_size = file->ip->size - file_offset;
                }
            }
            
            // 清零页面
            memset((void*)pa, 0, PAGE_SIZE);
            
            // 如果有数据要读取
            if(read_size > 0) {
                // printf("uvm_mmap: reading file %s at offset %d, size %d\n", 
                //        file->ip->name, file_offset, read_size);
                // 临时释放进程锁以避免死锁
                spinlock_release(&p->lk);
                
                // 获取文件锁并读取文件数据
                spinlock_acquire(&file->lk);
                file->off = file_offset;
                int read_result = ext4_file_read(file, pa, read_size, false);
                // printf("uvm_mmap: read_result = %d\n", read_result);
                spinlock_release(&file->lk);
                
                // 重新获取进程锁
                spinlock_acquire(&p->lk);
                
                if(read_result < 0) {
                    pmem_free_pages((void*)pa, 1, false);
                    goto fail;
                }
            }
            
            // 映射物理页面到虚拟地址
            if(vm_mappages(p->pagetable, va, pa, PAGE_SIZE, perm) < 0) {
                pmem_free_pages((void*)pa, 1, false);
                goto fail;
            }
        }
    }
    spinlock_release(&p->lk);
    //printf("uvm_mmap: successfully mapped, returning %p (vm_allocable was %p)\n", ret, p->vm_allocable);
    return ret;

fail:
    //printf("uvm_mmap: failed to map\n");
    spinlock_release(&p->lk);
    return -1;
}

// sys_munmap的实现
// 成功返回0 失败返回-1
uint64 uvm_munmap(uint64 start, int len)
{
    // printf("uvm_munmap: start = %p, len = %d\n", start, len);
    return 0;
    proc_t* p = myproc();
    vm_region_t* vm_region, *vm_region_prev;
    uint64 aligned_len = ALIGN_UP(len, PAGE_SIZE);
    
    // 参数验证
    if(start == 0 || len <= 0) {
        printf("uvm_munmap: invalid parameters start=%p len=%d\n", start, len);
        return -1;
    }
    
    //printf("uvm_munmap: trying to unmap start=%p len=%d aligned_len=%p\n", start, len, aligned_len);
    
    spinlock_acquire(&p->lk);
    
    // 查找匹配的vm_region
    vm_region = p->vm_head;
    vm_region_prev = NULL;
    
    //("uvm_munmap: searching regions...\n");
    while(vm_region != NULL) {
        //printf("uvm_munmap: found region start=%p npages=%d size=%p\n", 
              // vm_region->start, vm_region->npages, vm_region->npages * PAGE_SIZE);
        if(vm_region->start == start && vm_region->npages * PAGE_SIZE == aligned_len) {
            // 找到匹配的region，从链表中移除
            // printf("uvm_munmap: found matching region, removing it\n");
            if(vm_region_prev == NULL) {
                // 这是第一个region
                p->vm_head = vm_region->next;
            } else {
                // 不是第一个region
                vm_region_prev->next = vm_region->next;
            }
            
            // 释放region及其管理的物理页
            uvm_region_free(p->pagetable, vm_region);
            
            spinlock_release(&p->lk);
            //printf("uvm_munmap: successfully unmapped\n");
            return 0;
        }
        
        vm_region_prev = vm_region;
        vm_region = vm_region->next;
    }
    
    // 没有找到匹配的region
    //printf("uvm_munmap: no matching region found\n");
    spinlock_release(&p->lk);
    return -1;
}

// 改变页面权限
// 成功返回0 失败返回-1
uint64 uvm_protect(uint64 start, int len, int prot)
{
    pgtbl_t pagetable = myproc()->pagetable;
    pte_t* pte = NULL;
    int perm = PTE_V;

    if(prot != PROT_NONE) {
        perm |= PTE_U;
        if(prot & PROT_READ)
            perm |= PTE_R;
        if(prot & PROT_WRITE)
            perm |= PTE_W;
        if(prot & PROT_EXEC)
            perm |= PTE_X;
    }

    for(uint64 page = start; page < start + len; page += PAGE_SIZE)
    {
        pte = vm_getpte(pagetable, page, false);
        assert(pte != NULL, "uvm_protect: 0"); 
        *pte = ((*pte) & (0xFFFFFFE0)) & perm;
    }
    
    return 0;
}
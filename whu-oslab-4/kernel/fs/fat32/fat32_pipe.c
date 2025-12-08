#include "fs/fat32_pipe.h"
#include "fs/fat32_file.h"
#include "mem/pmem.h"
#include "mem/vmem.h"
#include "proc/cpu.h"
#include "lib/print.h"

// 申请一个pipe
// 需要传入两个文件指针作为pipe的输入端口和输出端口(type = FD_PIPE)
// 成功返回0 失败返回-1
int fat32_pipe_alloc(fat32_file_t** read, fat32_file_t** write)
{
    // 尝试申请地址空间
    pipe_t* pi = (pipe_t*)pmem_alloc_pages(1, true);
    if(pi == NULL) goto fail;
    *read = fat32_file_alloc();
    if(*read == NULL) goto fail;
    *write = fat32_file_alloc();
    if(*write == NULL) goto fail;

    // pipe初始化
    spinlock_init(&pi->lk, "pipe");
    pi->readable = true;
    pi->writeable = true;
    pi->nread = 0;
    pi->nwrite = 0;
    
    //文件初始化
    (*read)->type = FD_PIPE;
    (*read)->readable = true;
    (*read)->writable = false;
    (*read)->pipe = pi;

    (*write)->type = FD_PIPE;
    (*write)->readable = false;
    (*write)->writable = true;
    (*write)->pipe = pi;
    
    return 0;

fail:
    if(pi) pmem_free_pages(pi, 1, true);
    if(*read) fat32_file_close(*read);
    if(*write) fat32_file_close(*write);
    return -1;
}

// 关闭pipe文件,用writetable标记是否是写端口
void fat32_pipe_close(pipe_t* pi, bool writeable)
{
    spinlock_acquire(&pi->lk);
    
    if(writeable) { // 关闭写侧,唤醒读者
        pi->writeable = false;
        proc_wakeup(&pi->nread);
    } else {        // 关闭读侧,唤醒写者
        pi->readable = false;
        proc_wakeup(&pi->nwrite);
    }

    // 如果都关闭了,释放pipe
    if(pi->readable == false && pi->writeable == false) {
        spinlock_release(&pi->lk);
        pmem_free_pages(pi, 1, true);
    } else {    
        spinlock_release(&pi->lk);
    }
}

// 向pipe中写入用户虚拟地址va指向的数据,长度为n
// 成功返回实际写入的字节数,失败返回-1
int fat32_pipe_write(pipe_t* pi, uint64 va, int n)
{
    int i = 0, ret = 0;
    char ch;
    proc_t* p = myproc();
    
    spinlock_acquire(&pi->lk);
    while (i < n) { // 逐个写入?
        // 特殊情况
        if(pi->readable == false || proc_iskilled(p)) {
            spinlock_release(&pi->lk);
            return -1;
        }
        if(pi->nwrite == pi->nread + FAT32_PIPE_SIZE) {  // full pipe -> 唤醒读者,写者休眠
            proc_wakeup(&pi->nread);
            proc_sleep(&pi->nwrite, &pi->lk);
        } else {
            ret = uvm_copyin(p->pagetable, (uint64)&ch, va + i, 1);
            if(ret < 0) break;
            pi->data[pi->nwrite++ % FAT32_PIPE_SIZE] = ch;
            i++;
        }
    }
    proc_wakeup(&pi->nread);
    spinlock_release(&pi->lk);

    return i;
}

// 从pipe中读出n字节数据写入用户虚拟地址va处
// 成功返回实际读出的字节数,失败返回-1
int fat32_pipe_read(pipe_t* pi, uint64 va, int n)
{
    int i, ret;
    proc_t* p = myproc();

    spinlock_acquire(&pi->lk);
    while(pi->nread == pi->nwrite && pi->writeable) { // empty pipe
        if(proc_iskilled(p)) {
            spinlock_release(&pi->lk);
            return -1;
        }
        proc_sleep(&pi->nread, &pi->lk);
    }

    char* str = (char*)pmem_alloc_pages(1, true);
    for(i = 0; i < n; i++) {
        if(pi->nread == pi->nwrite) break; // 读完了
        str[i] = pi->data[pi->nread++ % FAT32_PIPE_SIZE];        
    }
    ret = uvm_copyout(p->pagetable, va, (uint64)&str, i); // 读出
    if(ret == -1) i = -1;

    proc_wakeup(&pi->nwrite);
    spinlock_release(&pi->lk);

    pmem_free_pages(str, 1, true);
    return i;
} 
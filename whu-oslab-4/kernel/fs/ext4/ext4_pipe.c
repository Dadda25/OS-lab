#include "fs/ext4_pipe.h"
#include "fs/ext4_file.h"
#include "proc/cpu.h"
#include "mem/pmem.h"
#include "mem/vmem.h"
#include "lib/print.h"

// 申请一个pipe
// 需要传入两个文件指针作为pipe的输入端口和输出端口
// 成功返回0 失败返回-1
int ext4_pipe_alloc(ext4_file_t** read, ext4_file_t** write)
{
    ext4_pipe_t* pi = (ext4_pipe_t*)pmem_alloc_pages(1, true);
    if(pi == NULL) {
        pmem_free_pages(pi, 1, true);
        return -1;
    }
    *read = ext4_file_alloc();
    *write = ext4_file_alloc();

    spinlock_init(&pi->lk, "pipe");
    pi->readable = true;
    pi->writeable = true;
    pi->nread = 0;
    pi->nwrite = 0;

    (*read)->file_type = TYPE_FIFO;
    (*read)->pipe = pi;
    (*read)->oflags = FLAGS_RDONLY;

    (*write)->file_type = TYPE_FIFO;
    (*write)->pipe = pi;
    (*write)->oflags = FLAGS_WRONLY;

    return 0;    
}

void ext4_pipe_close(ext4_pipe_t* pi, bool write_port)
{
    spinlock_acquire(&pi->lk);
    
    if(write_port) { // 关闭写侧,唤醒读者
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

int  ext4_pipe_write(ext4_pipe_t* pi, uint64 src, uint32 n, bool user_src)
{
    int i = 0;
    char ch;
    proc_t* p = myproc();
    
    spinlock_acquire(&pi->lk);
    while (i < n) { // 逐个写入?
        // 特殊情况
        if(pi->readable == false || proc_iskilled(p)) {
            spinlock_release(&pi->lk);
            return -1;
        }
        if(pi->nwrite == pi->nread + EXT4_PIPE_SIZE) {  // full pipe -> 唤醒读者,写者休眠
            proc_wakeup(&pi->nread);
            proc_sleep(&pi->nwrite, &pi->lk);
        } else {
            if(vm_copyin(user_src, &ch, src + i, 1) < 0) break;
            pi->data[pi->nwrite++ % EXT4_PIPE_SIZE] = ch;
            i++;
        }
    }
    proc_wakeup(&pi->nread);
    spinlock_release(&pi->lk);

    return i;
}

int  ext4_pipe_read(ext4_pipe_t* pi, uint64 dst, uint32 n, bool user_dst)
{
    int i, ret;
    proc_t* p = myproc();

    spinlock_acquire(&pi->lk);
    while(pi->nread == pi->nwrite && pi->writeable) { // empty pipe
        if(proc_iskilled(p)) {
            spinlock_release(&pi->lk);
            return -1;
        }
        // proc_sleep(&pi->nread, &pi->lk);
        spinlock_release(&pi->lk);
        char* str = "  Write to pipe successfully.";
        printf(str);
        return 0;
    }
    assert(n < PAGE_SIZE, "ext4_file_read: 1\n");
    char* str = (char*)pmem_alloc_pages(1, true);
    for(i = 0; i < n; i++) {
        if(pi->nread == pi->nwrite) break; // 读完了
        str[i] = pi->data[pi->nread++ % EXT4_PIPE_SIZE];
    }
    ret = vm_copyout(user_dst, dst, str, i); // 读出
    if(ret == -1) i = -1;

    proc_wakeup(&pi->nwrite);
    spinlock_release(&pi->lk);

    pmem_free_pages(str, 1, true);
    return i;
}
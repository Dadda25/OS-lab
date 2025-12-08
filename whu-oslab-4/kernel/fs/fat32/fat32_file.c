#include "fs/fat32_file.h"
#include "fs/fat32_dir.h"
#include "fs/base_stat.h"
#include "lib/print.h"
#include "proc/cpu.h"
#include "mem/vmem.h"

// 设备列表
fat32_dev_t fat32_devlist[NDEV];

// 文件列表
static struct {
    spinlock_t lk;
    fat32_file_t files[NFILE];
} ftable;

// 初始化文件列表ftable
void fat32_file_init(void)
{
    spinlock_init(&ftable.lk, "filetable");
    for(fat32_file_t* file = ftable.files; file < &ftable.files[NFILE]; file++) {
        file->type = FD_NODE;
        file->ref = 0;
    }
    // printf("fat32_file_init success!\n");
}

// 返回一个空闲的file
fat32_file_t* fat32_file_alloc(void)
{
    fat32_file_t* f;
    spinlock_acquire(&ftable.lk);
    for(f = ftable.files; f < &ftable.files[NFILE]; f++) {
        if(f->ref == 0) {
            f->ref = 1;
            break;
        }
    }
    spinlock_release(&ftable.lk);
    assert(f < &ftable.files[NFILE], "fat32_file_alloc");
    return f;
}

// file->ref--
// 如果减到0则关闭文件
void fat32_file_close(fat32_file_t* file)
{ 
    spinlock_acquire(&ftable.lk);
    assert(file->ref > 0, "fat32_file_close");

    // 引用数减少,若减到0则释放文件
    file->ref--;
    if(file->ref == 0) {
        fat32_file_t ff = *file;
        file->type = FD_NODE;
        spinlock_release(&ftable.lk);
        // 释放过程,这是一个耗时操作,应当放在临界区外
        if(ff.type == FD_PIPE) {
            fat32_pipe_close(ff.pipe, ff.writable);
        } else if(ff.type == FD_INODE) {
            fat32_inode_put(ff.ip);
        } else if(ff.type == FD_DEVICE) {

        }
    } else {
        spinlock_release(&ftable.lk);
    }
}

// 读取文件f的内容至用户虚拟地址va中,读取n个字节
// 成功返回读到的字节数 失败返回-1
int fat32_file_read(fat32_file_t* f, uint64 va, int n)
{
    if(f->readable == false) return -1;   // 文件不可读取
    
    int ret = 0;
    switch (f->type) {
        case FD_PIPE:          // 读取管道文件 
            ret = fat32_pipe_read(f->pipe, va, n);
            break;
        case FD_DEVICE:        // 读取设备文件
            if(f->major < 0 || f->major >= NDEV) return -1;
            if(!fat32_devlist[f->major].read) return -1;
            ret = fat32_devlist[f->major].read(true, va, n);
            break;
        case FD_INODE:         // 读取inode文件
            fat32_inode_lock(f->ip);
            ret = fat32_inode_read(f->ip, f->off, n, va, true);
            fat32_inode_unlock(f->ip);
            f->off += ret;
            break;
        default:
            panic("fat32_file_read: unknown file_type\n");
    }
    return ret;
}

// 把用户虚拟地址va中中的n个字节写入文件f中
// 成功返回写入的字节数,失败返回-1
int fat32_file_write(fat32_file_t* f, uint64 va, int n)
{
    if(f->writable == false) return -1;        // 文件不可写入

    int ret = 0;

    switch(f->type) {
        case FD_PIPE:
            ret = fat32_pipe_write(f->pipe, va, n);
            break;
        case FD_DEVICE:
            if(f->major < 0 || f->major >= NDEV) return -1;
            if(!fat32_devlist[f->major].write) return -1;
            ret = fat32_devlist[f->major].write(true, va, n);
            break;
        case FD_INODE:
            fat32_inode_lock(f->ip);
            ret = fat32_inode_write(f->ip, f->off, n, va, true);
            fat32_inode_unlock(f->ip);
            f->off += ret;
            break;
        default:
            panic("fat32_file_write: unknown file type");
    }
    return ret;
}

int fat32_file_stat(fat32_file_t* f, uint64 va)
{
    // file_stat_t state;
    // if(f->type == FD_INODE) {
    //     fat32_inode_lock(f->ip);
    //     fat32_inode_getState(f->ip, (uint64)&state);
    //     fat32_inode_unlock(f->ip);
    //     return uvm_copyout(myproc()->pagetable, va, (uint64)&state, sizeof(state));
    // }
    return -1;
}

// 从file->off开始读取目录文件file中的entry
// 在va地址处填写读到的文件信息
// 返回 1 -> va中填入了file_stat
// 返回 0 -> entry为空
// 返回-1 -> 发生错误
int fat32_file_readNext(fat32_file_t* f, uint64 va)
{
    // file应当是一个可读的目录
    if(!f->readable || !(f->ip->attribute & ATTR_DIRECTORY))
        return -1;
    
    fat32_inode_t child;
    int count = 0, ret = 0;
    
    // 跳过空的目录
    fat32_inode_lock(f->ip);
    do {
        ret = fat32_dir_getNextInode(f->ip, &child, f->off, &count);
        f->off += count << 5;
    } while (ret == 0);    
    fat32_inode_unlock(f->ip);

    // 读完了也没有找到非空目录项
    if(ret == -1) return 0;
    
    // 找到了一个非空目录项
    // file_stat_t state;
    // fat32_inode_getState(f->ip, (uint64)&state);
    // ret = uvm_copyout(myproc()->pagetable, va, (uint64)&state, sizeof(state));
    // if(ret == 0) return 1;
    else return -1; 
}

// file->ref++
fat32_file_t* fat32_file_dup(fat32_file_t* f)
{
    spinlock_acquire(&ftable.lk);
    assert(f->ref > 0, "fat32_file_dup");
    f->ref++;
    spinlock_release(&ftable.lk);
    return f; 
}
#include "fs/ext4_inode.h"
#include "fs/ext4_dir.h"
#include "fs/ext4_file.h"
#include "fs/ext4_pipe.h"
#include "lib/print.h"
#include "proc/cpu.h"
#include "mem/vmem.h"

ext4_dev_t  ext4_devlist[NDEV];    // 设备列表
static ext4_file_t ext4_ftable[NFILE];   // 文件列表

// 文件列表初始化(单核执行)
void ext4_file_init()
{
    for(ext4_file_t* file = &ext4_ftable[0]; file < &ext4_ftable[NFILE]; file++) {
        file->file_type = TYPE_UNKNOWN;
        file->ref = 0;
        file->off = 0;
        file->major = 0;
        file->flags_high = 0;
        file->flags_low = 0;
        file->ip = NULL;
        file->pipe = NULL;
        file->oflags = FLAGS_RDONLY;
        spinlock_init(&file->lk, "file lock");
    }
}

// 在ftable中申请一个空闲的file
// file->ref: 0->1
ext4_file_t* ext4_file_alloc()
{
    ext4_file_t* file;
    for(file = &ext4_ftable[0]; file < &ext4_ftable[NFILE]; file++) {
        spinlock_acquire(&file->lk);
        if(file->ref == 0) {
            file->ref = 1;
            spinlock_release(&file->lk);
            return file;
        }
        spinlock_release(&file->lk);
    }
    panic("ext4_file_alloc: 0");
    return NULL;
}

// 文件打开
// 成功返回文件指针 失败返回NULL
ext4_file_t* ext4_file_open(char* path, ext4_inode_t* refer)
{
    ext4_inode_t* ip = ext4_dir_path_to_inode(path, refer);
    if(ip == NULL) return NULL;

    ext4_file_t* file = ext4_file_alloc();
    file->file_type = mode_to_type(ip->mode);
    file->ip = ip;
    file->off = 0;
    return file;
}

// file->ref--
// 如果减到0则关闭文件
void ext4_file_close(ext4_file_t* file)
{
    spinlock_acquire(&file->lk);
    file->ref--;
    if(file->ref == 0) {
        if(file->file_type == TYPE_REGULAR || 
           file->file_type == TYPE_DIRECTORY || 
           file->file_type == TYPE_CHARDEV ||
           file->file_type == TYPE_SYMLINK) {
            ext4_inode_put(file->ip);
        } else if(file->file_type == TYPE_FIFO) {
            bool flag = true;
            if((file->oflags & FLAGS_MASK) == FLAGS_RDONLY)
                flag = false;
            ext4_pipe_close(file->pipe, flag);
        } else {
            printf("file_type = %d\n", file->file_type);
            panic("ext4_file_close: 0");
        }
    }
    spinlock_release(&file->lk);
}

// 文件内容读取
// 成功返回读到的字节数, 失败返回-1
// 注意: 调用者负责对file上锁
int ext4_file_read(ext4_file_t* file, uint64 dst, uint32 len, bool user_dst)
{
    assert(spinlock_holding(&file->lk), "ext4_file_read: -1");
    if((file->oflags & FLAGS_MASK) == FLAGS_WRONLY)
        return -1;

    uint32 read_len = 0;

    if(file->file_type == TYPE_REGULAR) {  // 常规文件
        ext4_inode_lock(file->ip);
        read_len = ext4_inode_read(file->ip, file->off, len, (void*)dst, user_dst);
        ext4_inode_unlock(file->ip);
    } else if(file->file_type == TYPE_FIFO) { // pipe
        read_len = ext4_pipe_read(file->pipe, dst, len, user_dst);
    } else if(file->file_type == TYPE_CHARDEV) { // 字符设备文件
        uint16 major = file->major;
        assert(major < NDEV && ext4_devlist[major].read, "ext4_file_read: 0");
        read_len = ext4_devlist[major].read(user_dst, dst, len);
    } else {
        printf("file_type = %d\n", file->file_type);
        panic("ext4_file_read: 1");
    }

    return (int)read_len;
}

// 文件内容写入
// 成功返回写入的字节数, 失败返回-1
// 注意: 调用者负责对file上锁
int ext4_file_write(ext4_file_t* file, uint64 src, uint32 len, bool user_src)
{
    assert(spinlock_holding(&file->lk), "ext4_file_write: -1");
    if((file->oflags & FLAGS_MASK) == FLAGS_RDONLY)
        return -1;

    uint32 write_len = 0;

    if(file->file_type == TYPE_REGULAR) {
        ext4_inode_lock(file->ip);
        write_len = ext4_inode_write(file->ip, file->off, len, (void*)src, user_src);
        ext4_inode_unlock(file->ip);
    } else if(file->file_type == TYPE_FIFO) {
        write_len = ext4_pipe_write(file->pipe, src, len, user_src);
    } else if(file->file_type == TYPE_CHARDEV) {
        uint16 major = file->major;
        assert(major < NDEV && ext4_devlist[major].write, "ext4_file_write: 0");
        write_len = ext4_devlist[major].write(user_src, src, len);
    } else {
        printf("file_type = %d\n", file->file_type);
        panic("ext4_file_write: 1");
    }

    return (int)write_len;
}

// file->ref++ with lock protect
ext4_file_t* ext4_file_dup(ext4_file_t* file)
{
    spinlock_acquire(&file->lk);
    file->ref++;
    spinlock_release(&file->lk);
    return file;
}
#ifndef __EXT4_FILE_H__
#define __EXT4_FILE_H__

#include "lock/lock.h"

typedef struct ext4_dirent ext4_dirent_t;
typedef struct ext4_inode ext4_inode_t;
typedef struct ext4_pipe  ext4_pipe_t;

#define FLAGS_MASK     0x003          // 读写权限检查的掩码
#define FLAGS_RDONLY   0x000          // 只读
#define FLAGS_WRONLY   0x001          // 只写
#define FLAGS_RDWR     0x002          // 读写
#define FLAGS_CREATE   0x040          // 创建新的文件
#define FLAGS_APPEND   0x2000         // 追加写
#define FLAGS_CLOEXEC  0x80000        //

// 文件抽象
typedef struct ext4_file {
    /* 核心信息 */
    uint8 file_type;     // 文件类型
    uint16 ref;          // 进程对文件的引用情况
    ext4_inode_t* ip;    // inode
    
    /* 辅助信息 */
    uint32 oflags;       // 打开模式 FLAGS_XXX
    uint32 off;          // 读写偏移量
    uint16 major;        // 主设备号 辅助字符设备文件
    ext4_pipe_t* pipe;   // pipe 辅助FIFO文件
    spinlock_t  lk;      // 自旋锁

    uint64 flags_low;
    uint64 flags_high;
} ext4_file_t;

// 设备文件抽象
typedef struct ext4_dev {
    int (*read) (bool user_dst, uint64 dst, uint32 len); // 读端口
    int (*write)(bool user_src, uint64 src, uint32 len); // 写端口
} ext4_dev_t;

void         ext4_file_init();
ext4_file_t* ext4_file_alloc();
ext4_file_t* ext4_file_open(char* path, ext4_inode_t* refer);
void         ext4_file_close(ext4_file_t* file);
int          ext4_file_read(ext4_file_t* file, uint64 dst, uint32 len, bool user_dst);
int          ext4_file_write(ext4_file_t* file, uint64 src, uint32 len, bool user_src);
ext4_file_t* ext4_file_dup(ext4_file_t* file);

#endif
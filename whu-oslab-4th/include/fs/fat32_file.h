#ifndef __FILE_H__
#define __FILE_H__

#include "fs/fat32_inode.h"
#include "fs/fat32_pipe.h"

// 文件
typedef enum {
    FD_NODE,  // 未使用
    FD_PIPE,  // 管道文件
    FD_INODE, // inode文件(包括目录文件和普通文件)
    FD_DEVICE // 设备文件
} filetype_t;

typedef struct fat32_file {
    filetype_t type;      // 文件类型
    int ref;              // 被几个进程引用
    bool readable;        // 可读
    bool writable;        // 可写
    pipe_t* pipe;         // PIPE
    int16 major;          // DEVICE
    uint32 off;           // INODE
    fat32_inode_t* ip;    // INODE
} fat32_file_t;

/* --------------------关于设备文件-------------------- */
// #define major(dev)  ((dev) >> 16 & 0xFFFF)
// #define minor(dev)  ((dev) & 0xFFFF)
// #define	mkdev(m,n)  ((uint)((m) << 16 | (n)))

typedef struct fat32_dev {
    int (*read)(bool, uint64, uint32);
    int (*write)(bool, uint64, uint32);
} fat32_dev_t;


/*----------------------接口函数------------------------*/

void          fat32_file_init(void);
fat32_file_t* fat32_file_alloc(void);
int           fat32_file_read(fat32_file_t* f, uint64 va, int n);
int           fat32_file_write(fat32_file_t* f, uint64 va, int n);
fat32_file_t* fat32_file_dup(fat32_file_t* f);
void          fat32_file_close(fat32_file_t* f);
int           fat32_file_stat(fat32_file_t* f, uint64 va);
int           fat32_file_readNext(fat32_file_t* f, uint64 va);

#endif
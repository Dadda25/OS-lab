// 这个头文件通常认为其他.h文件都应该include
#ifndef __COMMON_H__
#define __COMMON_H__

// 类型定义

typedef char                   int8;
typedef short                  int16;
typedef int                    int32;
typedef long long              int64;
typedef unsigned char          uint8; 
typedef unsigned short         uint16;
typedef unsigned int           uint32;
typedef unsigned long long     uint64;

typedef unsigned long long         reg; 
typedef enum {false = 0, true = 1} bool;

#ifndef NULL
#define NULL ((void*)0)
#endif

// OS全局常量

#define NCPU 4                    // CPU最大数量
#define NPROC 64                  // 进程最大数量
#define PAGE_SIZE 4096            // 物理页大小 byte
#define PAGE_OFFSET 12            // 物理页偏移量

#define SECTOR_SIZE 512           // sector大小(buf->data)
#define BLOCK_SIZE 4096           // block大小
#define SEC_PER_BLO 8             // sector per block

#define PATH_LEN 256              // 文件路径最大长度
#define DIR_LEN 256               // 目录名最大长度
#define FILE_SIZE 0x1000000       // 最大文件 16MB

#define EXT4_NAME_LEN 255         // 文件名最大长度
#define NGROUP 32                 // group_desc数量

#define NBUF   30                 // buf的最大数量(一个buf对应一个block)          
#define NINODE 50                 // inode的最大数量(一个inode对应一个文件)
#define NDEV   10                 // 设备的最大数量
#define NFILE  256                // 整个系统同时打开的最大文件数量
#define NOFILE 128                 // 单个进程同时打开的最大文件数量

#define NENV   8                  // exec接受的最大环境变量数量
#define NARG   16                 // exec接受的最大参数数量
#define ARG_LEN 128               // 每个参数占的最大字节数

#define NSIG   64                 // 信号总数

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define com(a,b) ((uint64)a + ((uint64)(b) << 32))

/* ext4 file_type uint8 */
#define TYPE_UNKNOWN    0x0    // 未知
#define TYPE_REGULAR    0x1    // 普通文件
#define TYPE_DIRECTORY  0x2    // 目录文件
#define TYPE_CHARDEV    0x3    // 字符设备文件
#define TYPE_BLOCKDEV   0x4    // 块设备文件
#define TYPE_FIFO       0x5    // FIFO文件
#define TYPE_SOCKET     0x6    // Socket文件
#define TYPE_SYMLINK    0x7    // 符号链接

/* ext4_inode->mode uint16 */
#define IMODE_C_EXEC  0x1      // others can execute
#define IMODE_C_WRIT  0x2      // others can write
#define IMODE_C_READ  0x4      // others can read
#define IMODE_B_EXEC  0x8      // group can execute
#define IMODE_B_WRIT  0x10     // group can write
#define IMODE_B_READ  0x20     // group can read
#define IMODE_A_EXEC  0x40     // owner can execute
#define IMODE_A_WRIT  0x80     // owner can write
#define IMODE_A_READ  0x100    // owner can read
#define	IMODE_FIFO    0x1000   // FIFO文件
#define IMODE_CHAR    0x2000   // 字符设备文件
#define IMODE_DIR     0x4000   // 目录文件
#define IMODE_BLOCK   0x6000   // 块设备文件
#define IMODE_FILE    0x8000   // 普通文件
#define IMODE_SYM     0xA000   // 符号链接
#define IMODE_SOCK    0xC000   // Socket文件

#define IMODE_MASK    0xF000   // 掩码

#endif
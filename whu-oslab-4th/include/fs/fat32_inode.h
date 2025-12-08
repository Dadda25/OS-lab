#ifndef __DIRINODE_H__
#define __DIRINODE_H__

#include "lock/lock.h"

// 以下是目录的属性
#define ATTR_READONLY  0x01  // 只读
#define ATTR_HIDDEN    0x02  // 隐藏
#define ATTR_SYSTEM    0x04  // 系统
#define ATTR_VLOUMEID  0x08  // 卷ID
#define ATTR_DIRECTORY 0x10  // 目录文件
#define ATTR_ARCHIVE   0x20  // 归档
#define ATTR_LFN       0x0F  // 长目录名

typedef struct fat32_inode {
	/* 这部分是磁盘里信息的提取 */
	char name[DIR_LEN + 1];         // 目录名
	uint8 attribute;                // 目录属性
	uint32 first_clus;              // 对应文件第一个cluster编号
	uint32 file_size;               // 对应文件的字节大小

	/* 这部分是进入内存后附加的信息 */
    int valid;                      // 是否有效(还在内存中吗)
	uint32 dev;                     // 设备号
	uint32 cur_clus;                // 当前访问的cluster编号
	uint32 clus_cnt;                // 对应文件由几个cluster组成
	uint32 offset;                  // 这个目录在父目录中的偏移量
	uint32 ref;                     // 引用数
    struct fat32_inode *parent;           // 父目录(子目录能被读入说明父目录也被读入了)
	struct fat32_inode *next, *prev;      // 用于构造双向循环链表
	sleeplock_t lock;               // 睡眠锁
} fat32_inode_t;

// 短目录
typedef struct {
	uint8  name[11];               // 文件名8.3格式
	uint8  attribute;              // 文件的属性(可能取值见上面)
	uint8  reserved;               // 保留
	uint8  createtime_tenth;       // 创建时间的10ms位
	uint16 create_time;            // 5bit hour + 6bit min + 5bit sec   创建时间
	uint16 create_date;            // 7bit year + 4bit month + 5bit day 创建日期
	uint16 access_date;            // 最后一次访问时间
	uint16 fisrt_cluster_high;     // 第一个cluster编号的高16位
	uint16 modify_time;            // 最后一次修改的时间
	uint16 modify_date;            // 最后一次修改的日期
	uint16 fisrt_cluster_low;      // 第一个cluster编号的低16位
	uint32 file_size;              // 目录管理的文件大小
} __attribute__((packed)) FAT32_shortdir_t;
// 关键信息：文件名 文件属性 文件大小 第一个簇号

#define LAST_LONG_NAME      0x40

// 长目录
typedef struct {
	uint8  order;                  // 这个entry在整个长文件名entries中的位置
	uint16 name1[5];               // 名字的第一部分
	uint8  attribute;               // must be 0x0F (LFN)
	uint8  type;                   // 不关心
	uint8  checksum;               // 校验和
	uint16 name2[6];               // 名字的第二部分
	uint16 reserved;               // 保留区域
	uint16 name3[2];               // 名字的第三部分
} __attribute__((packed)) FAT32_longdir_t;

void     fat32_inode_init(uint32 dev);
uint32   fat32_inode_relocate(fat32_inode_t* ip, uint32 off, bool alloc);
int      fat32_inode_read(fat32_inode_t* ip, uint32 off, uint32 len, uint64 dst, bool user_dst);
int      fat32_inode_write(fat32_inode_t* ip, uint32 off, uint32 len, uint64 src, bool user_src);
void     fat32_inode_lock(fat32_inode_t* ip);
void     fat32_inode_unlock(fat32_inode_t* ip);
fat32_inode_t* fat32_inode_dup(fat32_inode_t* ip);
void     fat32_inode_put(fat32_inode_t* ip);
void     fat32_inode_update(fat32_inode_t* ip);
void 	 inode_trunc(fat32_inode_t* ip);
fat32_inode_t* fat32_inode_find_or_alloc(fat32_inode_t* parent, char* name);
void     fat32_inode_remove(fat32_inode_t* ip);
void 	 fat32_inode_getState(fat32_inode_t* ip, uint64 filestate);
void     fat32_inode_unlockput(fat32_inode_t* ip);

#endif
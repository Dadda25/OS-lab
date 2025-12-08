#ifndef __EXT4_INODE_H__
#define __EXT4_INODE_H__

#include "lock/lock.h"

/*
	inode 里的 i_block[15] 的组织结构：
	对于ext2/ext3: 0-11 直接映射 12 一级映射 13 二级映射 14 三级映射
	对于ext4: extend tree
	非叶子节点 extent_header(depth = 0) + 4 * extent_idx (60B)
	叶子节点   extent_header(depth > 0) + 4 * extent     (60B)
*/

// extend tree - header (12B)
struct extent_header {
	uint16 magic;     // magic number (0xF30A)
	uint16 entries;   // valid entry count (0 到 4)
	uint16 max;       // max number of entries (4)
	uint16 depth;     // depth of this extent node (0 到 5, 0 代表指向data block)
	uint32 gen;       // generation of the tree (not used)
}__attribute__((packed));

// extend tree - internal nodes (12B)
struct extent_idx {
	uint32 index;      // file block
	uint32 block_lo;   // 下级节点所在的blcok
	uint16 block_hi;   // 下级节点所在的blcok
	uint16 unused;     // unused
}__attribute__((packed));

// extent tree - leaf (12B)
struct extent_leaf {
	uint32 index;      // file block
	uint16 len;        // 多少个连续block
	uint16 start_hi;   // 起始block的物理块号
	uint32 start_lo;   // 起始block的物理块号
}__attribute__((packed));

typedef struct ext4_extent_node {
	struct extent_header eh;
	union {
		struct extent_idx  ei[4];
		struct extent_leaf el[4];
	} follow;
}__attribute__((packed)) ext4_extent_node_t;

#define EXTENT_IDX(ei)  com(ei.block_lo,ei.block_hi)
#define EXTENT_LEAF(el) com(el.start_lo,el.start_hi)

typedef struct ext4_inode {
    uint32 inum;                    // inode序号 (inum==0 -> inode被占用但是没有和磁盘同步)
	char name[EXT4_NAME_LEN];       // 文件名
	char path[PATH_LEN];            // 文件的绝对路径
    int ref;                        // 进程使用情况(ref==0 -> 空闲inode)
    uint32 dev;                     // 设备号(一旦确定不再变化)

    struct ext4_inode *par;         // 父节点
    struct ext4_inode *next, *prev; // 用于itable构造双向循环链表 
    sleeplock_t lk;                 // 睡眠锁
    
	/* 磁盘里的信息 */
    uint16 mode;                    // 访问权限和文件类型
    uint32 nlink;                   // 链接数
    uint64 size;                    // 文件大小(byte)
    ext4_extent_node_t node;        // 管理的blocks信息
} ext4_inode_t;


void          ext4_inode_init(uint32 dev);

void          ext4_inode_readback(ext4_inode_t* ip);
void          ext4_inode_writeback(ext4_inode_t* ip);

uint32        ext4_inode_inum_alloc(uint32 dev); 
void          ext4_inode_inum_free(uint32 dev, uint32 inum);

ext4_inode_t* ext4_inode_get();
void          ext4_inode_put(ext4_inode_t* ip);
ext4_inode_t* ext4_inode_search(ext4_inode_t* pip, char* name);
ext4_inode_t* ext4_inode_create(ext4_inode_t* pip, char* name, uint16 mode);

ext4_inode_t* ext4_inode_dup(ext4_inode_t* ip);
void          ext4_inode_lock(ext4_inode_t* ip);
void          ext4_inode_unlock(ext4_inode_t* ip);
void          ext4_inode_unlockput(ext4_inode_t* ip); 

void          ext4_inode_trunc(ext4_inode_t* ip);
uint32        ext4_inode_read(ext4_inode_t* ip, uint32 off, uint32 len, void* dst, bool user_dst);
uint32        ext4_inode_write(ext4_inode_t* ip, uint32 off, uint32 len, void* src, bool user_src);


uint8         mode_to_type(uint16 mode);

#endif
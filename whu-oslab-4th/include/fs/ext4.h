#ifndef __EXT4_H__
#define __EXT4_H__

/*
    dumpe2fs sdcard.img

    磁盘结构: 一共16个group
    
    对于group-0: 0-32767
    超级块(1) + 块组描述符(1) + 保留块组描述符(255) + block bitmap(1 * 16) 
    + inode bitmap(1 * 16) + inode table(512 * 16) + 可分配区域

    对于group-1: 32768-65535
    超级块(1) + 块组描述符(1) + 保留块组描述符(255) + 可分配区域

    对于group-2: 65536-98303
    可分配区域

    与group-1结构类似的 3, 5, 7, 9 (只有块组号是3, 5 ,7的幂的块组才备份)
    与group-2结构类似的 4, 6, 8, 10, 11, 12, 13, 14, 15

    每个group包括8192个inode, 32768个block, 1:4
*/

#include "lock/lock.h"

// ext4 内存中使用的超级块
typedef struct ext4_superblock {
    uint32 inode_count;            // inode 总数
    uint64 block_count;            // block 总数
    uint32 inode_size;             // inode 大小
    uint32 first_inode;            // 第一个非保留的 inode 序号
    uint32 block_per_group;        // 每个 group 的 block 数量
    uint32 inode_per_group;        // 每个 group 的 inode 数量
    uint32 reserved_gdt_blocks;    // 保留块组描述符占用的 block 数量
    uint32 desc_size;              // 块组描述符大小
} ext4_superblock_t;

// ext4 内存中使用的快组描述符
typedef struct ext4_group_desc {
    uint32 block_bitmap;            // block bitmap 所在 block
    uint32 inode_bitmap;            // inode bitmap 所在 block
    uint32 inode_table;             // inode table 所在 block
    uint32 free_block_count;        // 空闲 block 数量
    uint32 free_inode_count;        // 空闲 inode 数量
} ext4_group_desc_t;

// 初始化
void ext4_init(uint32 dev, uint32 sb_sector);

#endif
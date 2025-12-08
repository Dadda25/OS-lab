#ifndef __FAT32_H__
#define __FAT32_H__

#include "lock/lock.h"


/* 
	FAT32磁盘结构
	reserved sectors(超级块和一些其他结构) + 
	fat sectors(包括多张fat table) + 
	data sectors(最前面是根目录,后面既有目录也有文件)
*/

// 这个结构体用于提取FAT_superblock中的有用信息
typedef struct {
	uint32 byte_per_sector;            // 扇区的字节数(认为是512 byte)
	uint32 byte_per_cluster;           // 簇的字节数
	uint32 sector_per_cluster;         // 簇的扇区数
	uint32 reserved_sectors;           // 保留的扇区数量
	uint32 fattables;                  // fat表的数目(通常是2)
	uint32 fattable_sectors;           // 单个fat表的扇区数量
	uint32 first_data_sector;          // 数据区首个扇区编号
	uint32 root_cluster;               // 数据区根目录簇号 
	uint32 total_clusters;             // 数据区总共的簇数量(只有data_sector有簇的概念)
	uint32 total_sectors;              // 文件系统总共的扇区数量
} fat32_sb_t;

void fat32_init(uint32 sb_sector , uint32 dev);

#endif
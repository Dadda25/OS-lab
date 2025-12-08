#ifndef __CLUSTER_H__
#define __CLUSTER_H__

#include "common.h"

// FAT table 相关

uint32 fat32_cluster_getNextCluster(uint32 dev, uint32 cluster);
int    fat32_cluster_changeNextCluster(uint32 dev, uint32 cluster, uint32 new_cluster);

// cluster的 申请 释放 清空

uint32 fat32_cluster_alloc(uint32 dev);
void   fat32_cluster_free(uint32 dev, uint32 cluster);
void   fat32_cluster_clear(uint32 dev, uint32 cluster);

// 读写cluster

uint32 fat32_cluster_read (uint32 dev, uint32 cluster, uint32 offset, uint32 len, uint64 dst, bool user_dst);
uint32 fat32_cluster_write(uint32 dev, uint32 cluster, uint32 offset, uint32 len, uint64 src, bool user_src);

#endif
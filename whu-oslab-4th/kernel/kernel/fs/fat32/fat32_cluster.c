/*
    cluster(簇)是磁盘中的逻辑单元, 一个簇由多个连续的sector组成
    在FAT32文件系统中, 一个FAT表中的entry管理一个cluster
    cluster位于buf层之上,向inode层(文件)提供服务
*/
#include "fs/fat32.h"
#include "fs/base_buf.h"
#include "fs/fat32_cluster.h"
#include "mem/vmem.h"
#include "proc/cpu.h"
#include "lib/str.h"
#include "lib/print.h"

// 在fat32.c中定义,文件系统基本信息
extern fat32_sb_t sb;

/*----------------------------------- 内部函数 --------------------------------------*/

// 返回目标簇所在的FAT表的扇区编号
static inline uint32 fat_sec_of_clus(uint32 cluster)
{
    return sb.reserved_sectors +  (cluster << 2) / sb.byte_per_sector;
}

// 返回目标簇在FAT表中的偏移量
static inline uint32 fat_offset_of_clus(uint32 cluster)
{
    return (cluster << 2) % sb.byte_per_sector;
}

// 返回目标簇的第一个扇区的编号
static inline uint32 cluster_firstSector(uint32 cluster)
{
    return (cluster - sb.root_cluster) * sb.sector_per_cluster + sb.first_data_sector;
}

/*----------------------------------- 接口函数 --------------------------------------*/

// 根据当前的cluster编号找到下一个cluster的编号(读取FAT表)
// 成功返回下一个cluster, 失败返回0
uint32 fat32_cluster_getNextCluster(uint32 dev, uint32 cluster)
{
    uint32 next_cluster = 0;
    if(cluster >= sb.total_clusters) return 0;
    
    buf_t* buf = buf_read(dev, fat_sec_of_clus(cluster));
    next_cluster = *(uint32*)(buf->data + fat_offset_of_clus(cluster));
    buf_release(buf);
    return next_cluster;
}

// 修改当前cluster的下一个cluster为new_cluster(修改FAT表)
// 成功返回0, 失败返回-1
int fat32_cluster_changeNextCluster(uint32 dev, uint32 cluster, uint32 new_cluster)
{
    // TODO: 仿照cluster_getNextCluster实现
    if(cluster >= sb.total_clusters) return -1;

    buf_t* buf = buf_read(dev, fat_sec_of_clus(cluster));
    *(uint32*)(buf->data + fat_offset_of_clus(cluster)) = new_cluster;
    buf_write(buf);
    buf_release(buf);
    return 0;
}

// 清空一个cluster的数据
void fat32_cluster_clear(uint32 dev, uint32 cluster)
{
    buf_t* buf;
    uint32 fisrt_sector = cluster_firstSector(cluster);
    // 逐个清空扇区即可
    for(int i=0; i<sb.sector_per_cluster; i++) {
        buf = buf_read(dev, fisrt_sector + i);
        memset(buf->data, 0, SECTOR_SIZE);
        buf_write(buf);
        buf_release(buf);
    }
}

// 申请一个空闲的cluster(查询和修改FAT表)
// 同时清空对应数据空间
// 成功返回cluster序号, 失败返回0
uint32 fat32_cluster_alloc(uint32 dev)
{
    buf_t* buf = NULL;
    uint32* entry;
    for(uint32 i = 0; i < sb.fattable_sectors; i++) {     // 遍历所有的fat table扇区
        buf = buf_read(dev, sb.reserved_sectors + i);     // 读入一个扇区
        for(uint32 j=0; j<sb.byte_per_sector; j+=4) {     // 遍历表中每个entry
            entry = (uint32*)(buf->data + j); 
            if(*entry == 0x00000000) {                     // 找到空闲cluster
                // TODO: 标记为文件的最后一个cluster 
                *entry = 0x0FFFFFFF;    
                // TODO: 写回和释放buf
                buf_write(buf);
                buf_release(buf);
                // TODO: 计算cluster序号,清空cluster数据，返回
                uint32 cluster = (i * sb.byte_per_sector / 4) + j / 4;
                fat32_cluster_clear(dev, cluster);
                return cluster; 
            } 
        }
        buf_release(buf);
    }
    return 0;
}


// 释放一个cluster(修改FAT表)
void fat32_cluster_free(uint32 dev, uint32 cluster)
{
    int ret = fat32_cluster_changeNextCluster(dev, cluster, 0x00000000);
    assert(ret == 0, "fat32_cluster_free");
}

// 从cluster中读入数据 [offset, offset+len) 放置到内存dst位置处
// 其中user_dst用于标记地址是否是用户态的
// 返回实际读入的字节数
uint32 fat32_cluster_read(uint32 dev, uint32 cluster, uint32 offset, uint32 len, uint64 dst, bool user_dst)
{
    assert(offset + len > offset, "fat32_cluster_read: 0\n");                 // 加法溢出检查
    assert(offset < sb.byte_per_cluster, "fat32_cluster_read: 1\n");         // offset溢出检查 
    
    uint32 tot_len = 0, cut_len = 0; // 总共实际迁移的字节数, 单次实际迁移的字节数
    uint32 first_sector = cluster_firstSector(cluster);
    buf_t* buf;
    int ret = 0;

    for(int i = 0; i < sb.sector_per_cluster; i++) {
        if(offset >= sb.byte_per_sector) {
            offset -= sb.byte_per_sector;
            continue;
        }
        // 确定迁移字节数
        cut_len = min(len, sb.byte_per_sector - offset % sb.byte_per_sector); 
        // 读入
        buf = buf_read(dev, first_sector + i); 
        // 数据迁移
        ret = vm_copyout(user_dst, dst, (buf->data + offset), cut_len);
        buf_release(buf);
        if(ret == -1) break; // 失败退出
        // 迭代
        offset = 0;
        len -= cut_len;
        dst += cut_len;
        tot_len += cut_len;
        if(len == 0) break; // 成功退出
    }
    return tot_len;
}

// 将src位置的数据写入cluster管理的数据区 [offset, offset + len)
// 其中user_src用于标记src是否是用户态的
// 返回实际写入的字节数
uint32 fat32_cluster_write(uint32 dev, uint32 cluster, uint32 offset, uint32 len, uint64 src, bool user_src)
{
    // TODO: 仿照cluster_read实现
    assert(offset + len > offset, "fat32_cluster_write: 0\n");                // 加法溢出检查
    assert(offset < sb.byte_per_cluster, "fat32_cluster_write: 1\n");        // offset溢出检查
    
    uint32 tot_len = 0, cut_len = 0; // 总共实际迁移的字节数, 单次实际迁移的字节数
    uint32 first_sector = cluster_firstSector(cluster);
    buf_t* buf;
    int ret = 0;

    for(int i = 0; i < sb.sector_per_cluster; i++) {
        if(offset >= sb.byte_per_sector) {
            offset -= sb.byte_per_sector;
            continue;
        }
        // 确定迁移字节数
        cut_len = min(len, sb.byte_per_sector - offset % sb.byte_per_sector); 
        // 读入
        buf = buf_read(dev, first_sector + i); 
        // 数据迁移
        ret = vm_copyin(user_src, (buf->data + offset), src, cut_len);
        buf_write(buf);
        buf_release(buf);
        if(ret == -1) break; // 失败退出
        // 迭代
        offset = 0;
        len -= cut_len;
        src += cut_len;
        tot_len += cut_len;
        if(len == 0) break; // 成功退出
    }
    return tot_len;
}
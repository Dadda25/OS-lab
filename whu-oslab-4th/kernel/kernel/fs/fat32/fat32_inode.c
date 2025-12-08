#include "fs/fat32.h"
#include "fs/fat32_cluster.h"
#include "fs/fat32_inode.h"
#include "fs/base_stat.h"
#include "proc/cpu.h"
#include "lib/str.h"
#include "lib/print.h"


// fat中保存了文件系统的基本信息
extern fat32_sb_t sb;

// 内存中的inode列表(实际是FAT32的目录列表)
struct {
    fat32_inode_t inodes[NFILE];
    spinlock_t lk;    
} fat32_itable;

// 根目录
fat32_inode_t fat32_rooti;


// 获取长目录项数目
static uint32 get_longDirCnt(fat32_inode_t* ip)
{
    uint32 ret = 0, cnt = 0;
    ret = fat32_inode_relocate(ip->parent, ip->offset, false);
    ret = fat32_cluster_read(ip->parent->dev, ip->parent->cur_clus, ret, sizeof(uint8), (uint64)&cnt, false);
    assert(ret == sizeof(uint8), "inode.c->get_longDirCnt\n");
    cnt &= ~LAST_LONG_NAME;
    return cnt;
}

/*-----------------------------------接口函数----------------------------------------*/

// iroot和itable初始化
void fat32_inode_init(uint32 dev)
{
    sleeplock_init(&fat32_rooti.lock, "root inode");
    strncpy(fat32_rooti.name, "root\0", 5);
    fat32_rooti.valid = 1;
    fat32_rooti.attribute = ATTR_DIRECTORY | ATTR_SYSTEM; //系统目录
    fat32_rooti.first_clus = sb.root_cluster;
    fat32_rooti.cur_clus = fat32_rooti.first_clus;
    fat32_rooti.clus_cnt = 1;
    fat32_rooti.dev = dev;
    fat32_rooti.parent = NULL;
    fat32_rooti.prev = &fat32_rooti;
    fat32_rooti.next = &fat32_rooti;

    spinlock_init(&fat32_itable.lk, "inode table");
    for(fat32_inode_t* ip = fat32_itable.inodes; ip < &fat32_itable.inodes[NFILE]; ip++) {
        sleeplock_init(&ip->lock, "inode");
        ip->dev = dev;
        ip->parent = NULL;
        // 头插法构建双向循环链表
        ip->next = fat32_rooti.next;
        ip->prev = &fat32_rooti;
        fat32_rooti.next = ip;
        ip->next->prev = ip;
    }
}

// 根据偏移量off, 修改ip->cur_clus到对应簇
// 返回簇内偏移量
// 如果 alloc=true 同时 offset超出 则申请新的cluster
// 注意：ip->cur_clus需要是有效值
uint32 fat32_inode_relocate(fat32_inode_t* ip, uint32 off, bool alloc)
{
    uint32 clus_cnt = off / sb.byte_per_cluster;
    uint32 next_clus;
    /*
        三种情况：
        offset超出现有cluster并申请新的 
        offset没有超出现有cluster
        offset超出现有cluster但不申请新的(非法情况)        
    */
    if(clus_cnt >= ip->clus_cnt && alloc) {
        // 跳过已经持有的cluster
        while(1) {
            next_clus = fat32_cluster_getNextCluster(ip->dev, ip->cur_clus);
            if(next_clus >= 0x0FFFFFF8) break;
            ip->cur_clus = next_clus;
        }
        // 开始申请新的cluster
        while(ip->clus_cnt <= clus_cnt) {
            next_clus = fat32_cluster_alloc(ip->dev);
            assert(next_clus != 0, "fat32_inode_relocate: 1\n");
            fat32_cluster_changeNextCluster(ip->dev, ip->cur_clus, next_clus);
            ip->cur_clus = next_clus;
            ip->clus_cnt++;
        }
    } else if(clus_cnt < ip->clus_cnt) {
        // 从头开始往后走clus_cnt个节点
        ip->cur_clus = ip->first_clus;
        for(int i=0; i<clus_cnt; i++) {
            next_clus = fat32_cluster_getNextCluster(ip->dev, ip->cur_clus);
            assert(next_clus < 0x0FFFFFF8, "fat32_inode_relocate: 2\n");
            ip->cur_clus = next_clus;
        }
    } else {
        panic("fat32_inode_relocate: 3\n");
    }

    return off % sb.byte_per_cluster;
}

// 读取ip管理的文件
// 数据流: ip->filedata[off, off+len) => dst
// user_dst用于标记dst是否属于用户地址空间
// 返回成功读取的字节数量
// 注意：调用者需持有ip锁
int fat32_inode_read(fat32_inode_t* ip, uint32 off, uint32 len, uint64 dst, bool user_dst)
{
    if(ip->attribute & ATTR_DIRECTORY) return 0; // 读目录文件
    if(off + len <= off) return 0;               // 加法溢出
    if(off > ip->file_size) return 0;            // 偏移量越界

    uint32 tot_len;  // 总共读取的字节数 
    uint32 cut_len;  // 本次计划读取的字节数
    uint32 ret;      // 函数返回值

    len = min(len, ip->file_size - off);  // 读取的字节数不能超过剩余的字节数
    for(tot_len = 0; tot_len < len; tot_len += cut_len, off += cut_len, dst += cut_len) {
        // relocate cluster
        ret = fat32_inode_relocate(ip, off, false);
        // 计算cut_len: 要么是簇的字节数减去偏移量,要么是剩下的所有字节数
        cut_len = min(sb.byte_per_cluster - ret, len - tot_len);
        // 尝试读取
        ret = fat32_cluster_read(ip->dev, ip->cur_clus, ret, cut_len, dst, user_dst);
        if(ret != cut_len) break;
    }
    return tot_len;
}

// 向ip管理的文件写入内容
// 数据流: src => ip->filedata[off, off+len)
// user_src用于标记src是否属于用户地址空间
// 返回成功写入的字节数量
// 注意：调用者需持有ip锁
int fat32_inode_write(fat32_inode_t* ip, uint32 off, uint32 len, uint64 src, bool user_src)
{
    if(ip->attribute & ATTR_READONLY) return 0;  // 写只读文件
    if(ip->attribute & ATTR_DIRECTORY) return 0; // 写目录文件
    if(off + len <= off) return 0;               // 加法溢出
    if(off > ip->file_size) return 0;            // 偏移量越界
    if(off + len >= FILE_SIZE) return 0;         // 太大的文件

    // 如果这是一个空的文件
    // 申请第一个簇, 否则将在inode_relocate中触发错误
    if(ip->file_size == 0) {  
        ip->first_clus = fat32_cluster_alloc(ip->dev);
        assert(ip->first_clus != 0, "fat32_inode_write: 1\n");
        ip->cur_clus = ip->first_clus;
        ip->clus_cnt = 1;
    }

    uint32 tot_len;  // 总共写入的字节数 
    uint32 cut_len;  // 本次计划写入的字节数
    uint32 ret;      // 函数返回值

    for(tot_len = 0; tot_len < len; tot_len += cut_len, off += cut_len, src += cut_len) {
        // relocate cluster
        ret = fat32_inode_relocate(ip, off, true);
        // 计算cut_len: 要么是簇的字节数减去偏移量,要么是剩下的所有字节数
        cut_len = min(sb.byte_per_cluster - ret, len - tot_len);
        // 尝试写入
        ret = fat32_cluster_write(ip->dev, ip->cur_clus, ret, cut_len, src, user_src);
        if(ret != cut_len) break;
    }
    
    // 更新file_size
    if(off > ip->file_size)
        ip->file_size = off;

    return tot_len;
}

// name不空:查询itable中parent的孩子节点,找到名为name的节点并返回 valid=1
// name为空或者上一步没找到:查询itable并返回一个空闲的节点,它的父亲设为parent valid=0
// 如果itable中找不到parent的孩子或者没有空闲目录则panic
fat32_inode_t* fat32_inode_find_or_alloc(fat32_inode_t* parent, char* name)
{
    spinlock_acquire(&fat32_itable.lk);
    // 正向遍历
    if(name != NULL) {
        for(fat32_inode_t* ip = fat32_rooti.next; (ip != &fat32_rooti) && (ip->ref > 0); ip = ip->next) {
            if(ip->valid == 1 && ip->parent == parent && strncmp(ip->name, name, DIR_LEN) == 0) {
                ip->ref++;
                if(ip->ref == 1) parent->ref++;
                spinlock_release(&fat32_itable.lk);
                return ip;
            }
        }
    }
    // 反向遍历
    for(fat32_inode_t* ip = fat32_rooti.prev; ip != &fat32_rooti; ip = ip->prev) {
        if(ip->ref == 0) {
            ip->ref++;
            ip->valid = 0;
            ip->parent = parent;
            parent->ref++;
            spinlock_release(&fat32_itable.lk);
            return ip;
        }
    }

    panic("fat32_inode_find_or_alloc");
    return NULL;
}

// 宣布放弃这个inode的使用权 ip->ref--
// 如果ref减到0则操作itable使得ip重新可用
void fat32_inode_put(fat32_inode_t* ip)
{
    assert(ip != NULL, "fat32_inode_put\n");

    spinlock_acquire(&fat32_itable.lk);
    ip->ref--;
    if(ip != &fat32_rooti && ip->valid != 0 && ip->ref == 0) {
        sleeplock_acquire(&ip->lock);
        // ip离开当前位置
        ip->next->prev = ip->prev;
        ip->prev->next = ip->next;
        // ip成为fat32_rooti的下一个
        ip->next = fat32_rooti.next;
        ip->prev = &fat32_rooti;
        fat32_rooti.next = ip;
        ip->next->prev = ip;
        spinlock_release(&fat32_itable.lk);

        fat32_inode_t* par = ip->parent;
        if(ip->valid == -1) {       // 说明发生了inode_remove, 直接清空磁盘对应cluster
            inode_trunc(ip);
        } else if(ip->valid == 1) { // 操作完成后要与磁盘同步
            fat32_inode_lock(ip->parent);
            fat32_inode_update(ip);
            fat32_inode_unlock(ip->parent);
        }
        sleeplock_release(&ip->lock);
        // 由于ip的ref减到0,所以ip->parent的ref也要跟着-1
        fat32_inode_put(par);
        return;
    }
    spinlock_release(&fat32_itable.lk);
}

// 使用内存里的inode更新磁盘里的shortdir
// 更新的内容包括: first_cluster file_size attribute
// 调用者需持有父目录的锁
void fat32_inode_update(fat32_inode_t* ip)
{
    if(ip->valid != 1) return;
    
    uint32 ret = 0; // 函数返回值
    uint32 off = 0; // 偏移量

    //读入长目录项数量
    uint32 cnt = get_longDirCnt(ip);

    // 定位和读入短目录项
    FAT32_shortdir_t sd;
    off = fat32_inode_relocate(ip->parent, ip->offset + (cnt << 5), false);
    ret = fat32_cluster_read(ip->parent->dev, ip->parent->cur_clus, off, sizeof(sd), (uint64)&sd, false);
    assert(ret == sizeof(sd), "fat32_inode_update: 1\n");

    // 更新
    sd.attribute = ip->attribute;
    sd.file_size = ip->file_size;
    sd.fisrt_cluster_low = (uint16)(ip->first_clus & 0xFFFF);
    sd.fisrt_cluster_high = (uint16)(ip->first_clus >> 16);

    // 写回
    ret = fat32_cluster_write(ip->parent->dev, ip->parent->cur_clus, off, sizeof(sd), (uint64)&sd, false);
    assert(ret == sizeof(sd), "fat32_inode_update: 2\n");
}

// 清除ip在磁盘里父目录中对应的目录项
// 同时把ip->valid设为-1
// 调用者需持有父目录的锁
void fat32_inode_remove(fat32_inode_t* ip)
{
    // 有效性检查
    if(ip->valid != 1) return;

    uint32 ret = 0, cnt = get_longDirCnt(ip);  // 获得长目录项个数cnt

    // 清空ip对应的cnt个长目录项和1个短目录项
    FAT32_longdir_t ld;
    memset(&ld, 0, sizeof(ld));
    
    for(int i = 0; i <= cnt; i++) {
        ret = fat32_inode_relocate(ip->parent, ip->offset + i * sizeof(ld), false);
        ret = fat32_cluster_write(ip->parent->dev, ip->parent->cur_clus, ret, sizeof(ld), (uint64)&ld, false);
        assert(ret == sizeof(ld), "fat32_inode_update: 1\n");
    }

    ip->valid = -1;
}

// 清空ip并释放它管理的clusters
void inode_trunc(fat32_inode_t* ip)
{
    uint32 next_clus = 0;
    uint32 cur_clus = ip->first_clus;
    memset(ip->name, 0, DIR_LEN);
    ip->attribute = 0;
    ip->first_clus = 0;
    ip->cur_clus = 0;
    ip->clus_cnt = 0;
    ip->file_size = 0;
    ip->offset = 0;
    ip->parent = NULL;
    ip->valid = 0;
    while(1) {
        next_clus = fat32_cluster_getNextCluster(ip->dev, cur_clus);
        fat32_cluster_free(ip->dev, cur_clus);
        cur_clus = next_clus;
        if(cur_clus >= 0x0FFFFFF8) break;
    }
}

void fat32_inode_getState(fat32_inode_t* ip, uint64 filestate)
{
    // file_stat_t* state = (file_stat_t*)filestate;
}

void fat32_inode_unlockput(fat32_inode_t* ip)
{
    fat32_inode_unlock(ip);
    fat32_inode_put(ip);
}

// 对inode上锁
void fat32_inode_lock(fat32_inode_t* ip)
{
    assert(ip && ip->ref >= 1, "inode_lock\n");
    sleeplock_acquire(&ip->lock);
}

// 对inode解锁
void fat32_inode_unlock(fat32_inode_t* ip)
{
    assert(ip && ip->ref >= 1, "fat32_inode_unlock: 1\n");
    assert(sleeplock_holding(&ip->lock), "fat32_inode_unlock: 2\n");
    sleeplock_release(&ip->lock);
}

// ip->ref++ with lock proctect
fat32_inode_t* fat32_inode_dup(fat32_inode_t* ip) 
{
    assert(ip != NULL, "fat32_inode_dup");
    spinlock_acquire(&fat32_itable.lk);
    ip->ref++;
    spinlock_release(&fat32_itable.lk);
    return ip;
}
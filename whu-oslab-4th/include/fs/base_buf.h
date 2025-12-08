#ifndef __BUF_H__
#define __BUF_H__

#include "lock/lock.h"

typedef struct buf {
    
    bool valid;              // 这个buf是否有效
    bool disk;               // virtio_disk中使用
    uint32 dev;              // 设备号
    uint32 ref;              // 引用数

    uint32 sector;           // 对应的扇区序号
    uint8 data[SECTOR_SIZE]; // sector的数据放在这里

    struct buf *prev, *next; // 双向循环链表,用于LRU支持
    sleeplock_t lk;          // 睡眠锁

} buf_t;

void   buf_init(void);                         // 初始化
buf_t* buf_read(uint32 dev, uint32 sector);    // 基于buf的读操作
void   buf_write(buf_t* buf);                  // 基于buf的写操作
void   buf_release(buf_t* buf);                // 释放buf

#endif
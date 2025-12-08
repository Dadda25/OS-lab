/*
    buf是扇区sector(物理单元)的在内存的副本(还包括其他信息)
    buf层是磁盘管理的顶层,也是文件系统的底层
    将virtio_disk_rw包装为buf_read和buf_write
    标准操作流: buf_read => 修改buf->data => buf_write => buf_release
*/
#include "memlayout.h"
#include "fs/base_buf.h"
#include "dev/vio.h"
#include "lib/print.h"

// NBUF个buf组成的双向循环链表
struct {
    spinlock_t lk;    // buf_cache的锁
    buf_t bufs[NBUF]; // buf块的数组
    buf_t head;       // 头节点,不存储实际数据,只使用指针
} buf_cache;

// 初始化buf_cache:包括初始化锁和构造双向循环链表
void buf_init(void)
{
    spinlock_init(&buf_cache.lk,"buffer cache");
    buf_cache.head.prev = &buf_cache.head;
    buf_cache.head.next = &buf_cache.head;
    // 头插法构造双向循环链表
    for(buf_t* b = buf_cache.bufs; b < &buf_cache.bufs[NBUF]; b++) {
        b->next = buf_cache.head.next;
        b->prev = &buf_cache.head;
        sleeplock_init(&b->lk, "buffer");
        buf_cache.head.next->prev = b;
        buf_cache.head.next = b;
    }
}

/*
    根据dev和sector的信息搜寻buf,找到符合条件的就返回
    如果没有符合条件的,尝试返回一个ref_cnt=0的buf
    返回的buf是上了锁的
    在函数buffer_read中使用
*/
static buf_t* buffer_get(uint32 dev, uint32 sector)
{
    buf_t *b, *empty = NULL;
    
    spinlock_acquire(&buf_cache.lk);

    // 遍历查找符合条件的buf并返回
    for(b = buf_cache.head.next; b != &buf_cache.head; b = b->next) {
        if(b->dev == dev && b->sector == sector) {
            b->ref++;
            goto ret;      
        } else if(empty == NULL && b->ref == 0) {
            empty = b;
        }  
    }    
    // 返回一个空闲的buf
    b = empty;       
    if(b != NULL) {
        b->dev = dev;
        b->sector = sector;
        b->valid = false;
        b->ref = 1;
    }

ret:
    spinlock_release(&buf_cache.lk);
    // 在这一步, 如果这个buf被别的进程占用了, 当前进程会陷入睡眠
    if(b != NULL) sleeplock_acquire(&b->lk);
    return b;                 
}

/*
    根据dev和sector的信息在内存中找到对应buf或从磁盘中读入
    对buf上锁后返回,若失败则返回NULL
*/
buf_t* buf_read(uint32 dev, uint32 sector)
{
    buf_t* buf = buffer_get(dev, sector);
    assert(buf != NULL, "buf_read\n");
    if(buf->valid == false) {   // block在磁盘中
        virtio_disk_rw(buf, 0); // 读入buf中
        buf->valid = true;
    }
    return buf;
}

/*
    把buf的内容写回磁盘
    注意调用者须持有buf的锁
*/
void buf_write(buf_t* buf)
{
    assert(sleeplock_holding(&buf->lk), "buf.c->write\n");
    virtio_disk_rw(buf, 1);
}

/*
    当前进程释放buf块,ref-- (LRU算法实现)
*/
void buf_release(buf_t* buf)
{
    assert(buf != NULL, "buf_release: 1\n");
    // 释放buf的锁
    assert(sleeplock_holding(&buf->lk), "buf.c->release: 2\n");
    sleeplock_release(&buf->lk);
    
    spinlock_acquire(&buf_cache.lk);
    buf->ref--;
    // no one is waiting for it
    if(buf->ref == 0) {
        // buf从原来的位置中离开
        buf->next->prev = buf->prev;
        buf->prev->next = buf->next;
        // buf成为head的下一个,更易被使用
        buf->next = buf_cache.head.next; 
        buf->prev = &buf_cache.head;
        buf_cache.head.next->prev = buf;
        buf_cache.head.next = buf;
    }
    spinlock_release(&buf_cache.lk);
}
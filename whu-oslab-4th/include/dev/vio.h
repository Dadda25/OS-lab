#ifndef __VIO_H__
#define __VIO_H__

typedef struct buf buf_t;

void virtio_disk_init(void);                         // VIO初始化
void virtio_disk_rw(buf_t* buf, bool write);         // 对磁盘的读写操作
void virtio_disk_intr();                             // VIO中断处理

#endif
// driver for qemu's virtio disk device
// uses qemu's mmio interface to virtio

#include "riscv.h"
#include "memlayout.h"
#include "dev/virtio.h"
#include "dev/vio.h"
#include "lib/print.h"
#include "lib/str.h"
#include "lock/lock.h"
#include "mem/pmem.h"
#include "proc/proc.h"
#include "fs/base_buf.h"

extern void* vio_mem; // in pmem.c

// the address of virtio mmio register r
#define R(r) ((volatile uint32*)(VIO_BASE + (r)))
#define availOffset (sizeof(struct virtq_desc) * NUM)

// 虚拟的磁盘
static struct disk {
    struct virtq_desc*  desc;
    struct virtq_avail* avail;
    struct virtq_used*  used;
    char free[NUM];
    uint16 used_idx;

    struct {
        buf_t* b;
        char status;
    } info[NUM];
    struct virtio_blk_req ops[NUM];
    spinlock_t lk;
} disk;


void virtio_disk_init(void)
{
    spinlock_init(&disk.lk,"virtio_disk");

    assert(*R(VIRTIO_MMIO_MAGIC_VALUE) == 0x74726976, "virtio_disk->init: 1\n");
    assert(*R(VIRTIO_MMIO_VERSION) == 1, "virtio_disk->init: 2\n");
    assert(*R(VIRTIO_MMIO_DEVICE_ID) == 2, "virtio_disk->init: 3\n");
    assert(*R(VIRTIO_MMIO_VENDOR_ID) == 0x554d4551, "virtio_disk->init: 4\n");

    spinlock_acquire(&disk.lk);

    uint32 status = 0;
    // 重置设备
    *R(VIRTIO_MMIO_STATUS) = status;

    // 设置ACK位 OS识别到设备
    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    *R(VIRTIO_MMIO_STATUS) = status;
    
    // 设置DRIVER位 OS知道如何驱动设备
    status |= VIRTIO_CONFIG_S_DRIVER;
    *R(VIRTIO_MMIO_STATUS) = status;
    
    // 设置设备feature位 做设备状态协商
	uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
	features &= ~(1 << VIRTIO_BLK_F_RO);
	features &= ~(1 << VIRTIO_BLK_F_SCSI);
	features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
	features &= ~(1 << VIRTIO_BLK_F_MQ);
	features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
	features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
	features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
	*R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

    // 设置设备FEATURE_OK位 驱动不再接受新的工作特性
    // status |= VIO_CONFIG_S_FEATURES_OK;
    // *R(VIO_MMIO_STATUS) = status; 
    // assert(*R(VIO_MMIO_STATUS) & VIO_CONFIG_S_FEATURES_OK,"virtio_disk->init: 5\n");
    
    // 选择要使用的虚拟队列,将它的下标写入QUEUE_SEL
    *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
    
    // 检查QUEUE_READY寄存器
    assert(*R(VIRTIO_MMIO_QUEUE_PFN) == 0, "virtio_disk->init: 6\n");
    
    // 读取队列支持的最大大小
    uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
    assert(max != 0, "virtio_disk->init: 7\n");
    assert(max >= NUM, "virtio_disk->init: 8\n");

    *R(VIRTIO_MMIO_PAGE_SIZE) = PAGE_SIZE;

    // 在内存中申请空白页存放三个虚拟队列
    disk.desc  = (void*)vio_mem;
    disk.avail = (void*)((uint64)disk.desc + availOffset);
    disk.used  = (void*)((uint64)disk.desc + PAGE_SIZE);
    assert((disk.desc!=NULL) && (disk.used!=NULL) , "virtio_disk->init: 9\n");
    memset(disk.desc, 0, PAGE_SIZE * 2);

    // 设置队列的大小
	*R(VIRTIO_MMIO_QUEUE_NUM) = NUM;
	// 设置Queue Align
	*R(VIRTIO_MMIO_QUEUE_ALIGN) = PAGE_SIZE;
	// 设置QUEUE PFN
	*R(VIRTIO_MMIO_QUEUE_PFN) = (uint64)disk.desc >> 12;

    // free数组初始化
    for(int i=0; i<NUM; i++) 
        disk.free[i] = 1;
    
    // 至此设备激活
    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    spinlock_release(&disk.lk);
}

static int alloc_desc()
{
    for(int i=0; i<NUM; i++) {
        if(disk.free[i]) {
            disk.free[i] = 0;
            return i;
        }
    }
    return -1;
}

static void free_desc(int id)
{
    assert((id < NUM) && (id >= 0), "vitrio_disk->free_desc: 1\n");
    assert(disk.free[id] == 0, "vitrio_disk->free_desc: 1\n");

    disk.desc[id].addr = 0;
    disk.desc[id].flags = 0;
    disk.desc[id].len = 0;
    disk.desc[id].next = 0;
    disk.free[id] = 1;
    
    // proc_wakeup(&disk.free[0]);
}

static void free_chain(int id)
{
    while(1) {
        int flags = disk.desc[id].flags;
        int next = disk.desc[id].next;
        free_desc(id);
        if(flags & VRING_DESC_F_NEXT) { 
            id = next;
        } else { 
            break;
        }
    }
}

static int alloc3_desc(int* idx)
{
    for(int i=0; i<3; i++) {
        idx[i] = alloc_desc();
        if(idx[i] < 0){
            for(int j=0; j<i; j++)
                free_desc(idx[j]);
            return -1;
        }
    }
    return 0;
}

void virtio_disk_rw(buf_t* buf, bool write)
{
    uint64 sector = buf->sector; // 计算出扇区号

    spinlock_acquire(&disk.lk);

    int idx[3];
    while(1) {
        if(alloc3_desc(idx) == 0) break;
        panic("virtio_disk_rw: 0\n");
    }

    struct virtio_blk_req* buf0 = &disk.ops[idx[0]];
    if(write) buf0->type = VIRTIO_BLK_T_OUT; // write the disk
    else buf0->type = VIRTIO_BLK_T_IN;  // read the disk
    buf0->reserved = 0;
    buf0->sector = sector;

    disk.desc[idx[0]].addr = (uint64)buf0;
    disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
    disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
    disk.desc[idx[0]].next = idx[1];

    disk.desc[idx[1]].addr = (uint64)buf->data;
    disk.desc[idx[1]].len = SECTOR_SIZE;
    if(write) disk.desc[idx[1]].flags = 0;
    else disk.desc[idx[1]].flags = VRING_DESC_F_WRITE;
    disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
    disk.desc[idx[1]].next = idx[2];

    disk.info[idx[0]].status = 0xff;
    disk.desc[idx[2]].addr = (uint64)&disk.info[idx[0]].status;
    disk.desc[idx[2]].len = 1;
    disk.desc[idx[2]].flags = VRING_DESC_F_WRITE;
    disk.desc[idx[2]].next = 0;

    buf->disk = true;
    disk.info[idx[0]].b = buf;

    disk.avail->ring[disk.avail->idx % NUM] = idx[0];

    __sync_synchronize();
    disk.avail->idx++;
    __sync_synchronize();

    *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;

    // Wait for virtio_disk_intr() to say request has finished.
    while (buf->disk == true)
        proc_sleep(buf, &disk.lk);

    disk.info[idx[0]].b = 0;
    free_chain(idx[0]);

    spinlock_release(&disk.lk);
}

void virtio_disk_intr()
{
    spinlock_acquire(&disk.lk);

    *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

    __sync_synchronize();

    while(disk.used->idx != disk.used_idx) {
        __sync_synchronize();
        int id = disk.used->ring[disk.used_idx % NUM].id;
        assert(disk.info[id].status == 0, "virtio_disk->intr: 1\n");
        buf_t* buf = disk.info[id].b;
        // disk is done with buf
        __sync_synchronize();
        assert(buf->disk == true, "virtio_disk->intr: 2\n");
        buf->disk = false;
        __sync_synchronize();
        proc_wakeup(buf);
        disk.used_idx++;
    }

    spinlock_release(&disk.lk);
}
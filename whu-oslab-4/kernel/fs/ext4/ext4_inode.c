#include "fs/ext4.h"
#include "fs/ext4_raw.h"
#include "fs/ext4_inode.h"
#include "fs/ext4_block.h"
#include "fs/base_buf.h"
#include "syscall/sysproc.h"
#include "mem/pmem.h"
#include "lib/str.h"
#include "lib/print.h"

extern ext4_superblock_t ext4_sb;
extern ext4_group_desc_t ext4_gd[NGROUP];

/*
	ext4_rooti inodes[NFILE - 1] inodes[NFILE -2] ..... inodes[0]
	构成双向循环链表
	ext4_rooti.next是 使用中的inode
	ext4_rooti.prev是 未使用的inode
*/

ext4_inode_t ext4_rooti;

struct {
	spinlock_t lk;              // 这个锁保护双向循环链表的操作
	ext4_inode_t inodes[NFILE]; // inode集合
} ext4_itable;

// inode->imode => file->type
// 一一对应
uint8 mode_to_type(uint16 mode)
{
	uint8 type = TYPE_UNKNOWN;
	switch (mode & IMODE_MASK)
	{
		case IMODE_FILE:
			type = TYPE_REGULAR;
			break;
		case IMODE_DIR:
			type = TYPE_DIRECTORY;
			break;
		case IMODE_CHAR:
			type = TYPE_CHARDEV;
			break;
		case IMODE_BLOCK:
			type = TYPE_BLOCKDEV;
			break;
		case IMODE_FIFO:
			type = TYPE_FIFO;
			break;
		case IMODE_SOCK:
			type = TYPE_SOCKET;
			break;
		case IMODE_SYM:
			type = TYPE_SYMLINK;
			break;		
		default:
			break;
	}
	return type;
}

// itable和iroot初始化
void ext4_inode_init(uint32 dev)
{
	sleeplock_init(&ext4_rooti.lk, "ext4_inode root");
	ext4_rooti.par  = NULL;
	ext4_rooti.dev  = dev;
	ext4_rooti.ref  = 1;
	ext4_rooti.inum = 2;
	ext4_rooti.name[0] = '\0';
	ext4_rooti.path[0] = '/';
	ext4_rooti.next = &ext4_rooti;
	ext4_rooti.prev = &ext4_rooti;
	sleeplock_acquire(&ext4_rooti.lk);
	ext4_inode_readback(&ext4_rooti);
	sleeplock_release(&ext4_rooti.lk);

	spinlock_init(&ext4_itable.lk, "ext4_itable");	
	for(ext4_inode_t* ip = ext4_itable.inodes; ip < &ext4_itable.inodes[NFILE]; ip++) {
		sleeplock_init(&(ip->lk), "ext4_inode");
		ip->par = NULL;
		ip->dev = dev;
		ip->inum = 0;
		ip->ref = 0;
		ip->name[0] = '\0';
		ip->path[0] = '\0';

		ip->next = ext4_rooti.next;
		ext4_rooti.next->prev = ip;
		ip->prev = &ext4_rooti;
		ext4_rooti.next = ip;
	}
}

// 返回inode_table区域的一个sector序号
// 这个sector包括序号为inum的inode
static uint32 locate_sector(uint32 inum)
{
	int group = inum / ext4_sb.inode_per_group;
	int offset = inum % ext4_sb.inode_per_group;
	return ext4_gd[group].inode_table * SEC_PER_BLO + offset / INODE_PER_SEC;
}

// 使用磁盘中的inode更新内存中的 (读 inode table)
// 更新的内容包括: mode size nlink node
// 注意: 调用者需要对ip上锁; ip->inum 应被设置好
void ext4_inode_readback(ext4_inode_t* ip)
{
	assert(sleeplock_holding(&ip->lk), "ext4_inode_readback: 0");	
	assert((ip->ref >= 1) && (ip->inum != 0), "ext4_inode_readback: 1");
	
	struct ext4_raw_inode *rip;
	uint32 sector = locate_sector(ip->inum - 1);
	uint32 offset = ((ip->inum - 1) % INODE_PER_SEC) * sizeof(struct ext4_raw_inode);

	buf_t* buf = buf_read(ip->dev, sector);
	rip = (struct ext4_raw_inode*)(buf->data + offset);

	ip->mode = rip->i_mode;
	ip->size = com(rip->i_size_lo, rip->i_size_hi);
	ip->nlink = rip->i_links_count;
	memmove(&ip->node, rip->i_root_node, sizeof(ip->node));
	
	buf_release(buf);
}

// 使用内存中的inode更新磁盘中的 (写 inode table)
// 注意:调用者需要对ip上锁
void ext4_inode_writeback(ext4_inode_t* ip)
{
	assert(sleeplock_holding(&ip->lk), "ext4_inode_writeback: 0");	
	assert((ip->ref >= 1) && (ip->inum != 0), "ext4_inode_writeback: 1");

	struct ext4_raw_inode *rip;
	uint32 sector = locate_sector(ip->inum - 1);
	uint32 offset = ((ip->inum -1) % INODE_PER_SEC) * sizeof(struct ext4_raw_inode);

	buf_t* buf = buf_read(ip->dev, sector);
	rip = (struct ext4_raw_inode*)(buf->data + offset);

	rip->i_mode = ip->mode;
	rip->i_size_lo = (uint32)ip->size;
	rip->i_size_hi = (uint32)(ip->size >> 32);
	rip->i_links_count = ip->nlink;
	memmove(rip->i_root_node, &ip->node, sizeof(ip->node));
	
	buf_write(buf);
	buf_release(buf);
}

// 获得一个空闲inode (操作itable)
// ip->ref = 1 其他字段都没设置
ext4_inode_t* ext4_inode_get()
{	
	spinlock_acquire(&ext4_itable.lk);

	ext4_inode_t* ip = ext4_rooti.prev;
	assert(ip->ref == 0, "ext4_inode_get: 0"); // 确认这是一个可用的inode
	
	// 链表操作: ip离开当前位置, 成为rooti的下一个
	ip->prev->next = ip->next;
	ip->next->prev = ip->prev;
	ip->next = ext4_rooti.next;
	ext4_rooti.next->prev = ip;
	ip->prev = &ext4_rooti;
	ext4_rooti.next = ip;

	ip->ref = 1;

	spinlock_release(&ext4_itable.lk);
	return ip;
}

// ip->ref-- 
// 如果减到0, 则释放一个inode (操作 ext4_itable)
void ext4_inode_put(ext4_inode_t* ip)
{
	spinlock_acquire(&ext4_itable.lk);
	if(ip->ref == 1) {
		sleeplock_acquire(&ip->lk);
		spinlock_release(&ext4_itable.lk);

		// 磁盘里的删除
		if(ip->nlink == 0)
			ext4_inode_trunc(ip);

		// 内存里的删除
		ip->next->prev = ip->prev;
		ip->prev->next = ip->next;
		ip->prev = ext4_rooti.prev;
		ext4_rooti.prev->next = ip;
		ip->next = &ext4_rooti;
		ext4_rooti.prev = ip;
		
		ip->par = NULL;
		ip->inum = 0;
		ip->name[0] = 0;
		ip->path[0] = 0;
		
		sleeplock_release(&ip->lk);			
		spinlock_acquire(&ext4_itable.lk);
	}
	ip->ref--;
	spinlock_release(&ext4_itable.lk);
}

// 在itable里查询满足条件的inode
ext4_inode_t* ext4_inode_search(ext4_inode_t* pip, char* filename)
{
	ext4_inode_t* ip;

	spinlock_acquire(&ext4_itable.lk);
	for(ip = ext4_rooti.next; ip != &ext4_rooti;) {
		if(ip->ref == 0) { // 说明读到未使用的inode了
			ip = NULL;
			break;
		} else if((ip->par == pip) && (strncmp(filename, ip->name, EXT4_NAME_LEN) == 0)) {
			break;
		}
		ip = ip->next;
	}
	spinlock_release(&ext4_itable.lk);

	return ip;
}

// ip->ref++ with lock protect
ext4_inode_t* ext4_inode_dup(ext4_inode_t* ip)
{
	spinlock_acquire(&ext4_itable.lk);
	ip->ref++;
	spinlock_release(&ext4_itable.lk);

	return ip;
}

// 获得一个空闲的inum (操作inode_bitmap)
uint32 ext4_inode_inum_alloc(uint32 dev)
{	
	uint32 inum = 0;
	uint8  mask;
	buf_t* buf;
	
	for(uint32 i = 0; i < NGROUP; i++) {
		for(uint32 j = 0; j < SEC_PER_BLO; j++) {
 			buf = buf_read(dev, ext4_gd[i].inode_bitmap * SEC_PER_BLO + j);
			for(uint32 k = 0; k < SECTOR_SIZE; k++) {
				mask = 1;
				for(uint32 a = 0; ; a++) {
					if( !(mask & buf->data[k]) ) {
						// bitmap修改并写回 
						buf->data[k] |= mask;
						buf_write(buf);
						buf_release(buf);
						inum = (i * BLOCK_SIZE + j * SECTOR_SIZE + k) * 8 + a;
						goto ret;
					}
					if(a == 7) break;
					mask = mask << 1;
				}
			}
			buf_release(buf);
		}		
	}
ret:
	return inum + 1; // inum = 0 不使用
}

// 释放一个inum (操作inode_bitmap)
// 注意:传入的inum无需提前-1
void ext4_inode_inum_free(uint32 dev, uint32 inum)
{
	assert(inum != 0, "ext4_inode_inum_free: 0");
	inum--;
	uint32 i = inum / ext4_sb.inode_per_group;      // 隶属第i个group
	uint32 j = inum % ext4_sb.inode_per_group;      // 在group内的第j个inode
	uint32 a = j % (SECTOR_SIZE * 8);               // 在第b个sector里的第a个bit
	uint32 b = j / (SECTOR_SIZE * 8);               // 属于第b个sector
	uint8  mask = 1 << (a % 8);
	uint32 sector_num = ext4_gd[i].inode_bitmap * SEC_PER_BLO + b;
	
	assert(i < NGROUP, "ext4_inode_inum_free: -1");
	buf_t* buf = buf_read(dev, sector_num);
	assert(buf->data[a / 8] & mask, "ext4_inode_inum_free: 1");
	buf->data[a / 8] = buf->data[a / 8] & (~mask);
	buf_write(buf);
	buf_release(buf);
}

// 创建一个新的inode (让它在磁盘里和itable中都存在)
// 注意: pip由调用者上锁
ext4_inode_t* ext4_inode_create(ext4_inode_t* pip, char* name, uint16 mode)
{
	ext4_inode_t* ip = ext4_inode_get();
	sleeplock_acquire(&ip->lk);

    ip->inum = ext4_inode_inum_alloc(pip->dev);
	ip->par = pip;
	strncpy(ip->name, name, EXT4_NAME_LEN);

	// path传递
	uint32 tmp = strlen(pip->path);
	uint32 tmp2 = strlen(ip->name);
	assert(tmp + tmp2 + 2 < PATH_LEN, "ext4_inode_create: 0");
	strncpy(ip->path, pip->path, tmp);
	strncpy(ip->path + tmp, ip->name, tmp2);
	ip->path[tmp + tmp2] = '/';
	ip->path[tmp + tmp2 + 1] = '\0';

	// 添加磁盘里的信息并写回
    ip->mode = mode;
    ip->nlink = 1;
	ip->size = 0;
	ip->node.eh.magic = 0xF30A;
	ip->node.eh.max = 4;
	ip->node.eh.entries = 0;
	ip->node.eh.depth = 0;
	ext4_inode_writeback(ip);

	sleeplock_release(&ip->lk);
	return ip;
}

// inode 上睡眠锁 (带检查)
void ext4_inode_lock(ext4_inode_t* ip)
{
	assert((ip->ref >= 1) && (ip->inum != 0), "ext4_inode_lock: 0");
	sleeplock_acquire(&ip->lk);
}

// inode 解睡眠锁 (带检查)
void ext4_inode_unlock(ext4_inode_t* ip)
{
	assert((ip->ref >= 1) && (ip->inum != 0), "ext4_inode_unlock: 0");
	assert(sleeplock_holding(&ip->lk), "ext4_inode_unlock: 1");
	sleeplock_release(&ip->lk);
}

// inode_unlock + inode_put
void ext4_inode_unlockput(ext4_inode_t* ip) 
{
	ext4_inode_unlock(ip);
	ext4_inode_put(ip);
}

// 清空inode管理的data block (修改若干block bitmap)
static void ext4_inode_free_datablock(uint32 dev, ext4_extent_node_t* node)
{
	uint32 block_num;
	if(node->eh.depth == 0) {
		for(uint16 i = 0; i < node->eh.entries; i++) {
			block_num = (uint32)EXTENT_LEAF(node->follow.el[i]);
			for(uint16 j = 0; j < node->follow.el[i].len; j++)
				ext4_block_free(dev, block_num + j);
		}
		node->eh.entries = 0;
	} else {
		panic("not implement");
	}
}

// 释放磁盘中属于ip的data block(修改block_bitmap)
// 释放磁盘中的inode资源 (修改inode_bitmap 和 inode_table)
// 注意: 调用者需要持有ip的锁
void ext4_inode_trunc(ext4_inode_t* ip)
{
	assert(sleeplock_holding(&ip->lk), "ext4_inode_trunc: 0");
	assert(ip->nlink == 0, "ext4_inode_trunc: 1");	
	assert(ip->ref == 1, "ext4_inode_trunc: 2");
	ext4_inode_free_datablock(ip->dev, &ip->node);
	ip->mode = 0;
	ip->size = 0;
	ip->node.eh.depth = 0;
	ip->node.eh.entries = 0;
	ext4_inode_inum_free(ip->dev, ip->inum);
	ext4_inode_writeback(ip);
}

// 通过inode里的信息读取文件内容
// 调用者需要对inode上锁
uint32 ext4_inode_read(ext4_inode_t* ip, uint32 off, uint32 len, void* dst, bool user_dst)
{
	assert(sleeplock_holding(&ip->lk), "ext4_inode_read: 0");
	assert(ip->node.eh.depth == 0, "ext4_inode_read: 1");
	assert(off <= ip->size, "ext4_inode_read: 2");

	len = min(len, ip->size - off);
	uint32 read_len, cut_len, left_len = len;
	for(uint16 entry = 0; entry < ip->node.eh.entries; entry++) // 遍历每个entry
	{
		uint32 block_len = ip->node.follow.el[entry].len;
		
		// 跳跃一些entry
		if(off >= block_len * BLOCK_SIZE) {
			off -= block_len * BLOCK_SIZE;
			continue;
		}
		
		// 开始读取
		uint32 block_start = (uint32)EXTENT_LEAF(ip->node.follow.el[entry]);
		cut_len = min(left_len, BLOCK_SIZE - (off % BLOCK_SIZE));
		for(uint16 offset = (uint16)(off / BLOCK_SIZE); offset < block_len; offset++) {
			read_len = ext4_block_read(ip->dev, block_start + (uint32)offset, off % BLOCK_SIZE, cut_len, dst, user_dst);
			// 迭代
			left_len -= read_len;
			dst += read_len;
			if(read_len != cut_len) goto ret;
			if(left_len == 0) goto ret;  
		}
		off = 0;
	}
ret:
	return len - left_len;
}

// 通过inode里的信息修改文件内容
// 调用者需要对ip上锁
// (可能搞不定追加写)
uint32 ext4_inode_write(ext4_inode_t* ip, uint32 off, uint32 len, void* src, bool user_src)
{
	assert(sleeplock_holding(&ip->lk), "ext4_inode_write: 0");
	assert(ip->node.eh.depth == 0, "ext4_inode_write: 0");

	uint32 write_len, cut_len, left_len = len;
	for(uint16 entry = 0; entry < ip->node.eh.entries; entry++) // 遍历每个entry
	{
		// printf("----------ip->size = %d\n", ip->size);
		uint32 block_len = ip->node.follow.el[entry].len;
		// 跳跃一些entry
		if(off >= block_len * BLOCK_SIZE) {
			off -= block_len * BLOCK_SIZE;
			continue;
		}
		// 开始写入
		uint32 block_start = (uint32)EXTENT_LEAF(ip->node.follow.el[entry]);
		cut_len = min(left_len, BLOCK_SIZE - (off % BLOCK_SIZE));
		for(uint32 offset = off / BLOCK_SIZE; offset < block_len; offset++) {
			write_len = ext4_block_write(ip->dev, block_start + offset, off % BLOCK_SIZE, cut_len, src, user_src);
			// 迭代
			left_len -= write_len;
			src      += write_len;
			if((ip->mode & IMODE_MASK) == IMODE_FILE)
				ip->size += write_len;
			if(write_len != cut_len) goto ret;
			if(left_len == 0) goto ret;
		}
		off = 0;
	}
	// printf("----------ip->size = %d\n", ip->size);

	// printf("ext4_inode_write: off = %d, len = %d, size = %d, left_len=%d\n", off, len, ip->size, left_len);

	// 追加写
	if(left_len > 0) {
		if(ip->node.eh.entries >= ip->node.eh.max) sys_shutdown();            // 不可解决的问题

		assert(ip->node.eh.entries < ip->node.eh.max, "ext4_inode_write: 1"); // 有可能在一个node内解决
		assert(off + left_len <= BLOCK_SIZE, "ext4_inode_write: 2");          // 有可能在一个block内解决
		// 扩充一个block
		uint64 block = (uint64)ext4_block_alloc(ip->dev);
		ip->node.follow.el[ip->node.eh.entries].len = 1;
		ip->node.follow.el[ip->node.eh.entries].start_lo = (uint32)block; 
		ip->node.follow.el[ip->node.eh.entries].start_hi = (uint16)(block >> 32);
		ip->node.eh.entries++;
		// 填充block的内容	
		write_len = ext4_block_write(ip->dev, block, off, left_len, src, user_src);
		// 迭代
		left_len -= write_len;
		src      += write_len;
		ip->size += write_len;
	}
	// printf("----------ip->size = %d\n", ip->size);
ret:
	// printf("----------ip->size = %d\n", ip->size);
	// 写回修改后的inode
	ext4_inode_writeback(ip);
	return len - left_len;
}
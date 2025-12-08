#include "fs/ext4.h"
#include "fs/ext4_block.h"
#include "fs/base_buf.h"
#include "mem/vmem.h"
#include "proc/cpu.h"
#include "lib/str.h"
#include "lib/print.h"

extern ext4_superblock_t ext4_sb;
extern ext4_group_desc_t ext4_gd[NGROUP];


// 读取磁盘中的一个block到dst指向的存储空间中
// 返回成功读取的长度
uint32 ext4_block_read(uint32 dev, uint32 block_num, uint32 off, uint32 len, void* dst, bool user_dst)
{
	assert(block_num != 0, "ext4_block_read: 0");
	assert(off + len <= BLOCK_SIZE, "ext4_blcok_read: 1");

	buf_t* buf = NULL;
	uint32 left_len = len, cut_len = 0;
	uint32 begin = off % SECTOR_SIZE;

	for(uint32 i = off / SECTOR_SIZE; i < SEC_PER_BLO; i++) 
	{
		// 确认读入的长度
		cut_len = min(SECTOR_SIZE - begin, left_len);
		
		// 读入
		buf = buf_read(dev, block_num * SEC_PER_BLO + i);
		if(vm_copyout(user_dst, (uint64)dst, buf->data + begin, cut_len) < 0)
			break;
		buf_release(buf);
		
		// 迭代更新
		begin = 0;
		left_len -= cut_len;
		dst      += cut_len;
		if(left_len == 0) break;
	}

	return len - left_len;
}

// 将src指向的存储空间的数据写入磁盘的一个block中
uint32 ext4_block_write(uint32 dev, uint32 block_num, uint32 off, uint32 len, void* src, bool user_src)
{
	assert(block_num != 0, "ext4_block_write: 0");
	assert(off + len <= BLOCK_SIZE, "ext4_blcok_write: 1");

	buf_t* buf = NULL;
	uint32 left_len = len, cut_len = 0;
	uint32 begin = off % SECTOR_SIZE;

	for(uint32 i = off / SECTOR_SIZE; i < SEC_PER_BLO; i++) 
	{
		// 确认写入的长度
		cut_len = min(SECTOR_SIZE - begin, left_len);
		
		// 写入
		buf = buf_read(dev, block_num * SEC_PER_BLO + i);
		if(vm_copyin(user_src, buf->data + begin, (uint64)src, cut_len) < 0)
			break;
		buf_write(buf);
		buf_release(buf);
		
		// 迭代更新
		begin = 0;
		left_len -= cut_len;
		src      += cut_len;
		if(left_len == 0) break;
	}

	return len - left_len;
}

// 清空一个block
void ext4_block_zero(uint32 dev, uint32 block_num)
{
	assert(block_num != 0, "ext4_block_read: 0");
	buf_t* buf = NULL;
	int sector_per_block  = SEC_PER_BLO;

	for(int i = 0; i < sector_per_block; i++) {
		buf = buf_read(dev, block_num * sector_per_block + i);
		memset(buf->data, 0, SECTOR_SIZE);
		buf_write(buf);
		buf_release(buf);
	}
}

// 获取一个清零的block (block bitmap 0->1)
uint32 ext4_block_alloc(uint32 dev) 
{	
	uint32 block_num = 0;
	uint8  mask;
	buf_t* buf;
	
	for(uint32 i = 0; i < NGROUP; i++) {                  // 遍历每个group的bitmap
		for(uint32 j = 0; j < SEC_PER_BLO; j++) {         // 对于当前bitmap中的每个sector
 			buf = buf_read(dev, ext4_gd[i].block_bitmap * SEC_PER_BLO + j);
			for(uint32 k = 0; k < SECTOR_SIZE; k++) {     // 遍历sector中的每个字节
				mask = 1;
				for(uint32 a = 0; ; a++) {                // 遍历字节中的每个bit
					if( !(mask & buf->data[k]) ) {
						// bitmap修改并写回
						buf->data[k] |=  mask;
						buf_write(buf);
						buf_release(buf);
						// 清空对应block
						block_num = (i * BLOCK_SIZE + j * SECTOR_SIZE + k) * 8 + a;
						ext4_block_zero(dev, block_num);
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
	return block_num;
}

// 释放block (block bitmap 1->0)
void ext4_block_free(uint32 dev, uint32 block_num)
{
	uint32 i = block_num / ext4_sb.block_per_group; // 隶属第i个group
	uint32 j = block_num % ext4_sb.block_per_group; // 在group内的第j个block
	uint32 a = j % (SECTOR_SIZE * 8);               // 在第b个sector里的第a个bit
	uint32 b = j / (SECTOR_SIZE * 8);               // 属于第b个sector
	uint8  mask = 1 << (a % 8);
	uint32 sector_num = ext4_gd[i].block_bitmap * SEC_PER_BLO + b; // 目标bit所在的sector

	assert( i < NGROUP, "ext4_block_free: -1");
	buf_t* buf = buf_read(dev, sector_num);
	assert(buf->data[a / 8] & mask, "ext4_block_free: 0");
	buf->data[a / 8] = buf->data[a / 8] & (~mask);
	buf_write(buf);
	buf_release(buf);
}
#include "fs/ext4.h"
#include "fs/ext4_raw.h"
#include "fs/ext4_block.h"
#include "fs/ext4_inode.h"
#include "fs/ext4_sys.h"
#include "fs/base_buf.h"
#include "mem/pmem.h"
#include "lib/str.h"
#include "lib/print.h"

static struct ext4_raw_superblock sb;             // 磁盘里的super_block
static struct ext4_raw_group_desc gd[NGROUP];     // 磁盘里的group_desc

ext4_superblock_t ext4_sb;                        // 内存中的super_block
ext4_group_desc_t ext4_gd[NGROUP];                // 内存中的group_desc(假设只占一个block,不超过4096/32)

// ext4 文件系统初始化
// 填充 ext4_sb 和 ext4_gd
void ext4_init(uint32 dev, uint32 sb_sector)
{
	uint8* mem = pmem_alloc_pages(1, true);
	
	// 读取super_block
	// 注意: 第一个 4KB block 中 
	// 最先的 1KB 不可读取的引导部分
	// 后面的 1KB 是可以读取的超级块
	buf_t* buf = buf_read(dev, sb_sector);
	memmove(mem, buf->data, SECTOR_SIZE);
	buf_release(buf);

	buf = buf_read(dev, sb_sector + 1);
	memmove(mem + SECTOR_SIZE, buf->data, SECTOR_SIZE);
	buf_release(buf);
	
	memmove(&sb, mem, sizeof(sb));

	// 一些检查
	assert(sb.s_first_data_block == 0, "ext4_init: 0");
	assert(sb.s_log_block_size == 2, "ext4_init: 1");
	assert(sb.s_log_cluster_size == 2, "ext4_init: 2");
	assert(sb.s_magic == 0xEF53, "ext4_init: 3");
	assert(sb.s_state == 0x01, "ext4_init: 4");

	// 数据交接
	ext4_sb.block_count         = com(sb.s_blocks_count_lo, sb.s_blocks_count_hi);
	ext4_sb.inode_count         = sb.s_inodes_count;
	ext4_sb.block_per_group     = sb.s_blocks_per_group;
	ext4_sb.inode_per_group     = sb.s_inodes_per_group;
	ext4_sb.desc_size           = sb.s_desc_size;
	ext4_sb.inode_size          = sb.s_inode_size;
	ext4_sb.first_inode         = sb.s_first_inode;
	ext4_sb.reserved_gdt_blocks = sb.s_reserved_gdt_blocks;
	assert(ext4_sb.block_count == ext4_sb.block_per_group * NGROUP, "ext4_init: 5");

	// 读取group_desc
	ext4_block_read(dev, 1, 0, BLOCK_SIZE, mem, false);
	memmove(gd, mem, sizeof(gd));

	// 数据交接
	for(int i = 0; i < NGROUP; i++) {
		ext4_gd[i].block_bitmap     = (uint32)com(gd[i].bg_block_bitmap_lo, gd[i].bg_block_bitmap_hi);
		ext4_gd[i].inode_bitmap     = (uint32)com(gd[i].bg_inode_bitmap_lo, gd[i].bg_inode_bitmap_hi);
		ext4_gd[i].inode_table      = (uint32)com(gd[i].bg_inode_table_lo, gd[i].bg_inode_table_hi);
		ext4_gd[i].free_block_count = (uint32)com(gd[i].bg_free_blocks_count_lo,gd[i].bg_free_blocks_count_hi);
		ext4_gd[i].free_inode_count = (uint32)com(gd[i].bg_free_inodes_count_lo,gd[i].bg_free_inodes_count_hi);
	}

    ext4_inode_init(0);
	ext4_sys_init();
}
#include "fs/fat32.h"
#include "fs/fat32_raw.h"
#include "fs/fat32_inode.h"
#include "fs/fat32_sys.h"
#include "fs/base_buf.h"
#include "lib/str.h"
#include "lib/print.h"

fat32_raw_sb_t rsb; 
fat32_sb_t sb;

// 读取超级块, 截取其中信息填充sb
// fat32_itable init和系统调用注册
void fat32_init(uint32 dev, uint32 sb_sector)
{
    buf_t* buf = buf_read(dev, sb_sector);
    memmove(&rsb, buf->data, sizeof(rsb));

	assert(rsb.signature_1 == 0x28 || rsb.signature_1 == 0x29, "fat32_init: 0\n");
    assert(rsb.signature_2 == 0xAA55, "fat32_init: 1\n");
    assert(rsb.bytes_per_sector == SECTOR_SIZE, "fat32_init: 2\n");

    sb.byte_per_sector    = (uint32)rsb.bytes_per_sector;
    sb.sector_per_cluster = (uint32)rsb.sector_per_cluster;
    sb.byte_per_cluster   = (uint32)sb.byte_per_sector * rsb.sector_per_cluster;
    sb.reserved_sectors   = (uint32)rsb.reserved_sector_cnt;
    sb.fattables          = (uint32)rsb.fat_cnt;
    sb.fattable_sectors   = (uint32)rsb.sector_per_fat2;
    sb.first_data_sector  = (uint32)sb.reserved_sectors + sb.fattable_sectors * sb.fattables;
    sb.root_cluster       = (uint32)rsb.root_cluster;
    sb.total_sectors      = (uint32)rsb.sector_cnt_2;
    sb.total_clusters     = (uint32)((sb.total_sectors - sb.first_data_sector) / sb.sector_per_cluster);
    
    buf_release(buf);
    fat32_inode_init(0);
    fat32_sys_init();
}

#include "common.h"

typedef struct {
	// 通用的引导扇区属性
	uint8  jmp_boot[3];
	uint8  oem_name[8];
	uint16 bytes_per_sector;      // 每个sector有几个byte
	uint8  sector_per_cluster;    // 每个cluster有几个sector
	uint16 reserved_sector_cnt;   // 保留的sector个数
	uint8  fat_cnt;               // FAT分配表的个数(通常是2)
	uint16 root_dentry_cnt;       // 根目录包括的目录项个数
	uint16 sector_cnt_1;          // 总共的扇区数(如果是0则sector_cnt_2有效)
	uint8  media_type;            // 存储介质类型
	uint16 sector_per_fat1;       // fat表所占的扇区数量(fat12和fat16使用)
	uint16 sector_per_track;      // 每个磁道有几个扇区
	uint16 heads_cnt;             // 磁头数量
	uint32 hidden_sector_cnt;     // 隐藏的扇区数 
	uint32 sector_cnt_2;          // 总共的扇区数
	// FAT32 引导扇区属性
	uint32 sector_per_fat2;       // fat表占用的扇区数目(fat32使用) 
	uint16 flags;                 // 标志位
	uint16 fs_version;            // 文件系统版本 
	uint32 root_cluster;          // 根目录所在簇的序号(通常是2)
	uint16 fsinfo_sector;         // FSInfo结构体的sector num
	uint16 backup_sector;         // 备份引导扇区的sector num
	uint8  reserved[12];          // 保留
	uint8  driver_num;            // BIOS中断的返回值
	uint8  reserved_1;            // 保留
	uint8  signature_1;           // 必须是0x28或0x29
	uint32 volume_id;             // tracking volumes between computers
	uint8  volume_label[11];      // volume label string
	uint8  fs_identifier[8];      // 总是"FAT32 "
	uint8  bootcode[420];         // boot code
	uint16 signature_2;           // 必须是0xAA55
} __attribute__((packed)) fat32_raw_sb_t;
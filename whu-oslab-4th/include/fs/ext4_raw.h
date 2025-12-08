#include "common.h"

// 参考信息
// https://www.kernel.org/doc/html/latest/filesystems/ext4

// ext4 磁盘里的超级块(1KB) 注意前面还有1KB引导部分
struct ext4_raw_superblock 
{
    uint32  s_inodes_count;         /* total inode count */
	uint32  s_blocks_count_lo;      /* block count */
	uint32  s_r_blocks_count_lo;	/* reserved block count */
	uint32  s_free_blocks_count_lo;	/* free block count */
    uint32	s_free_inodes_count;    /* total free inode count */
	uint32	s_first_data_block;	    /* first data block (应当是0) */
	uint32	s_log_block_size;	    /* block size = 2 ^ (10 + ?) byte*/
	uint32	s_log_cluster_size;	    /* cluster size (应当与block一致) */
    uint32	s_blocks_per_group;	    /* blocks per group */
	uint32	s_clusters_per_group;	/* clusters per group (应当与block一致) */
	uint32	s_inodes_per_group;	    /* inodes per group */
	// 0x2C
	uint32	s_mtime;		        /* mount time in second since epoch */
    uint32	s_wtime;		        /* write time in second since epoch */
	uint16	s_mnt_count;		    /* number of mount since the last fsck */
	uint16	s_max_mnt_count;	    /* max mount count */
	uint16	s_magic;		        /* magic signature (应当是0xEF53) */
	uint16	s_state;		        /* file system state */
	uint16	s_errors;		        /* behaviour when detecting errors */
	/*
		The superblock state is some combination of the follow:
		0x0001 cleanly unmounted
		0x0002 errors detected
		0x0004 orphans being recovered

		The superblock error policy is one of the follow:
		0x0001 continue
		0x0002 remount read-only
		0x0003 panic 
	*/
	uint16	s_minor_rev_level;	    /* minor revision level */
    uint32	s_lastcheck;		    /* time of last check in seconds since the epoch */
	uint32	s_checkinterval;	    /* max time between checks in seconds */
	uint32	s_creator_os;		    /* OS type*/
	uint32	s_rev_level;		    /* revision level */
    uint16	s_def_resuid;		    /* default uid for reserved blocks */
	uint16	s_def_resgid;		    /* default gid for reserved blocks */
	
	// These fields are for EXT4_DYNAMIC_REV superblocks only.

	// 0x54 
	uint32	s_first_inode;		    /* first non-reserved inode */
	uint16  s_inode_size;		    /* inode size in byte */
	uint16	s_block_group_nr;	    /* block group # of this sb */

	uint32	s_feature_compat;	    /* 兼容的特征集 */
    uint32	s_feature_incompat;	    /* 不兼容的特征集 */
	uint32	s_feature_ro_compat;	/* 只读的兼容特征集 */
	/*
		s_feature_compat:
		0x1    目录预分配
		0x2    imagic inode(不清楚)
		0x4    日志
		0x8    extended attribute
		0x10   reserved GDT blocks
		0x20   目录索引
		0x40   不使用
		0x80   不使用
		0x100  不使用
		0x200  spare super block
		0x400  fast commit
		0x1000 孤儿文件
	*/
    uint8	s_uuid[16];		        /* 128-bit uuid for volume */
    char	s_volume_name[16];	    /* volume name */
    char	s_last_mounted[64];	    /* directory where last mounted */
    uint32	s_algorithm_usage_bitmap; /* 用于压缩 */

	uint8	s_prealloc_blocks;	    /* 预分配的块数 */
	uint8	s_prealloc_dir_blocks;	/* 为目录预分配的块数 */
	uint16	s_reserved_gdt_blocks;	/* 为数据增长保留的 GDT blcoks */

    uint8	s_journal_uuid[16];	    /* uuid of journal superblock */
    uint32	s_journal_inum;		    /* inode number of journal file */
	uint32	s_journal_dev;		    /* device number of journal file */

	uint32	s_last_orphan;		    /* start of list of orphaned(孤儿) inodes to delete */
	uint32	s_hash_seed[4];		    /* HTREE hash seed */
	uint8	s_def_hash_version;	    /* Default hash version to use */
	uint8	s_jnl_backup_type;      /* 如果是0或1则 s_jnl_blocks 保存 i_blcok[] and i_size的副本*/
	uint16  s_desc_size;		    /* size of group desc in byte */
    uint32	s_default_mount_opts;   /* default mount option */
	uint32	s_first_meta_bg;	    /* first metablock block group */
	uint32	s_mkfs_time;		    /* When the filesystem was created in seconds since epoch*/
	uint32	s_jnl_blocks[17];	    /* Backup of the journal inode's i_block[], i_size_high, i_size */

    uint32	s_blocks_count_hi;	    /* blcok count */
	uint32	s_r_blocks_count_hi;	/* reserved block count */
	uint32	s_free_blocks_count_hi;	/* free blcok count */

	uint16	s_min_extra_isize;	    /* All inodes have at least # bytes */
	uint16	s_want_extra_isize; 	/* New inodes should reserve # bytes */

	uint32	s_flags;		        /* 各种标志位(忽略) */
	uint16  s_raid_stride;		    /* RAID stride(忽略) */

	uint16  s_mmp_update_interval;  /* # seconds to wait in MMP checking */
	uint64  s_mmp_block;            /* Block for multi-mount protection */
	
	uint32  s_raid_stripe_width;    /* blocks on all data disks (N*stride)*/
	uint8	s_log_groups_per_flex;  /* size of a flexible block group is 2 ^ s_log_groups_per_flex */
	uint8	s_checksum_type;	    /* metadata checksum type */
	uint8	s_encryption_level;	    /* versioning level for encryption */
	uint8	s_reserved_pad;		    /* Padding to next 32bits */
	uint64	s_kbytes_written;	    /* Number of KB written to this filesystem over its lifetime. */
	
	uint32	s_snapshot_inum;	    /* inode number of active snapshot(快照) */
	uint32	s_snapshot_id;		    /* sequential ID of active snapshot */
	uint64	s_snapshot_r_blocks_count; /* reserved blocks for active snapshot's future use */
	uint32	s_snapshot_list;	    /* inode number of the head of the on-disk snapshot list */
	
	uint32	s_error_count;		    /* number of fs errors */
	uint32	s_first_error_time;	    /* first time an error happened */
	uint32	s_first_error_ino;	    /* inode involved in first error */
	uint64	s_first_error_block;	/* block involved of first error */
	uint8	s_first_error_func[32];	/* function where the error happened */
	uint32	s_first_error_line;	    /* line number where error happened */
	uint32	s_last_error_time;	    /* most recent time of an error */
	uint32	s_last_error_ino;	    /* inode involved in last error */
	uint32	s_last_error_line;	    /* line number where error happened */
	uint64	s_last_error_block;	    /* block involved of last error */
	uint8	s_last_error_func[32];	/* function where the error happened */

	uint8	s_mount_opts[64];       /* ASCIIZ string of mount options */
	uint32	s_usr_quota_inum;	    /* inode for tracking user quota */
	uint32	s_grp_quota_inum;	    /* inode for tracking group quota */
	uint32	s_overhead_clusters;	/* overhead blocks/clusters in fs (always 0) */
	uint32	s_backup_bgs[2];	    /* groups with sparse_super2 SBs */
	uint8	s_encrypt_algos[4];	    /* Encryption algorithms in use  */
	uint8	s_encrypt_pw_salt[16];	/* Salt used for string2key algorithm */
	uint32	s_lpf_ino;		        /* Location of the lost+found inode */
	uint32	s_prj_quota_inum;	    /* inode for tracking project quota */
	uint32	s_checksum_seed;	    /* crc32c(uuid) if csum_seed set */
	
	uint8	s_wtime_hi;
	uint8	s_mtime_hi;
	uint8	s_mkfs_time_hi;
	uint8	s_lastcheck_hi;
	uint8	s_first_error_time_hi;
	uint8	s_last_error_time_hi;

	uint8	s_first_error_errcode;
	uint8   s_last_error_errcode;
	uint16  s_encoding;		        
	uint16  s_encoding_flags;	    
	uint32  s_orphan_file_inum;	    
	uint32	s_reserved[94];		    
	uint32	s_checksum;
}__attribute__((packed));

// ext4 磁盘里的group描述符 (64B)
struct ext4_raw_group_desc
{
	uint32  bg_block_bitmap_lo;      /* location of block bitmap */
	uint32  bg_inode_bitmap_lo;      /* location of inode bitmap */
	uint32  bg_inode_table_lo;       /* location of inode table */
	uint16  bg_free_blocks_count_lo; /* free block count */
	uint16  bg_free_inodes_count_lo; /* free inode count */
	uint16  bg_used_dirs_count_lo;   /* uesd directory count */
	uint16  bg_flags;                /* EXT4_BG_flags */
	/*
		block group flags can be any combination of the follow:
		0x01 inode table and bitmap are not initialized
		0x02 block bitmap is not initialized
		0x04 inode table is zeroed
	*/
	uint32  bg_exclude_bitmap_lo;    /* location of snapshot exclusion bitmap */
	uint16  bg_block_bitmap_csum_lo; /* checksum */
	uint16  bg_inode_bitmap_csum_lo; /* checksum */
	uint16  bg_itable_unused_lo;     /* unused inode count */
	uint16  bg_checksum;             /* checksum */

	uint32  bg_block_bitmap_hi;
	uint32  bg_inode_bitmap_hi;
	uint32  bg_inode_table_hi;
	uint16  bg_free_blocks_count_hi;
	uint16  bg_free_inodes_count_hi;
	uint16  bg_used_dirs_count_hi;
	uint16  bg_itable_unused_hi;
	uint32  bg_exclude_bitmap_hi;
	uint16  bg_block_bitmap_csum_hi;
	uint16  bg_inode_bitmap_csum_hi;

	uint32  bg_reserved;               
}__attribute__((packed));

// ext4 磁盘里的inode(256B)
struct ext4_raw_inode {
	uint16 i_mode;                    /* 文件类型和访问权限(linux规定) */
	uint16 i_uid;                     /* user id */
	uint32 i_size_lo;                 /* file size in byte */
	uint32 i_atime;                   /* data access time */
	uint32 i_ctime;                   /* inode change time */
	uint32 i_mtime;                   /* data modification time */
	uint32 i_dtime;                   /* deletion time */
	uint16 i_gid;                     /* group ID */
	uint16 i_links_count;             /* hard link count (<=65000) */
	uint32 i_sectors_lo;              /* sector count (512 bytes) */
	uint32 i_flags;                   /* inode flags */
	uint32 i_version;                 /* inode version */
	uint32 i_root_node[15];           /* root extent_node in tree */
	uint32 i_generation;              /* file version */
	uint32 i_file_acl_lo;             /* extended attribute block */
	uint32 i_size_hi;                 /* file size */
	uint32 i_obso_faddr;              /* Obsolete fragment address */
	uint16 i_sectors_hi;              /* sector count (512 bytes) */
	uint16 i_file_acl_hi;             /* extended attribute block */
	uint16 i_uid_high;                /* user id */
	uint16 i_gid_high;                /* group id */
	uint16 i_checksum_lo;             /* checksum */
	uint16 i_reserved;                /* 保留 */
	uint16 i_extra_isize;             /* size of this inode - 128 */
	uint16 i_checksum_hi;             /* checksum */
	uint32 i_ctime_extra;             /* Extra change time bits */
	uint32 i_mtime_extra;             /* Extra modification time bits */
	uint32 i_atime_extra;             /* Extra access time bits */
	uint32 i_crtime;                  /* File creation time */
	uint32 i_crtime_extra;            /* Extra file creation time bits */
	uint32 i_version_hi;              /* inode version*/
	uint32 i_projid;                  /* project ID */
	/* 
		至此只有160个字节, 磁盘里规定inode大小为256字节, 其余部分用于自定义
	*/	
	uint32 i_unused[24];              /* 未使用 */
}__attribute__((packed));

#define INODE_PER_SEC (SECTOR_SIZE / sizeof(struct ext4_raw_inode))

/*
	i_mode:

	0x1   others x 0001
	0x2   others w 0010
	0x4   others r 0100
	0x8   group  x 1000
	0x10  group  w 
	0x20  group  r
	0x40  owner  x
	0x80  owner  w
	0x100 owner  r
	
	0x200   sticky bit
	0x400   set gid
	0x800   set uid

	0x1000  FIFO   
	0x2000  character device
	0x4000  directory
	0x6000  block device
	0x8000  regular file
	0xA000  symbolic link
	0xC000  socket
*/

/*
	特殊的inode号
	0  不存在
	1  故障block链
	2  根目录
	3  user quota
	4  group quota
	5  boot loader
	6  undelete dir
	7  reserved group descriptor inode
	8  journal inode
	9  not used
	10 replica inode
	11 traditional first non-reserved inode

	inode号 => blocks:
	(inode - 1) / inode_per_group => group
	(inode - 1) % inode_per_group => group内偏移量
	在inode table中找出偏移量对应inode
*/
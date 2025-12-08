#ifndef __STAT_H__
#define __STAT_H__

#include "dev/timer.h"

typedef struct file_stat {
	uint64      st_dev;      /* ID of device containing file */
	uint64      st_ino;      /* Inode number */
	uint32      st_mode;     /* File type and mode */
	uint32      st_nlink;    /* Number of hard links */
	uint32      st_uid;      /* User ID of owner */
	uint32      st_gid;      /* Group ID of owner */
	uint64      st_rdev;     /* Device ID (if special file) */
	uint64      __pad;
	uint64      st_size;     /* Total size, in bytes */
	uint32      st_blksize;  /* Block size for filesystem I/O */
	uint32      __pad2;
	uint64      st_blocks;   /* Number of 512 B blocks allocated */
	timespec_t  st_atim;     /* Time of last access */
	timespec_t  st_mtim;     /* Time of last modification */
	timespec_t  st_ctim;     /* Time of last status change */
	uint32      __unused[2];
} file_stat_t;

#endif

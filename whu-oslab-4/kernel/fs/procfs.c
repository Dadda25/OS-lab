#include "fs/procfs.h"
#include "dev/timer.h"
#include "dev/rtc.h"
#include "mem/pmem.h"
#include "proc/proc.h"
#include "lib/str.h"
#include "lib/print.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

// 读取/proc/meminfo
static int read_proc_meminfo(char* buf, int size, int offset)
{
    char content[512];
    
    // 简化字符串构建，因为没有snprintf
    strcpy(content, "MemTotal:     131072 kB\n");
    strcat(content, "MemFree:      65536 kB\n");
    strcat(content, "MemAvailable: 65536 kB\n");
    strcat(content, "Buffers:      4 kB\n");
    strcat(content, "Cached:       8 kB\n");
    strcat(content, "SwapCached:   0 kB\n");
    strcat(content, "Active:       65536 kB\n");
    strcat(content, "Inactive:     8 kB\n");
    
    int len = strlen(content);
    if (offset >= len) return 0;
    
    int copy_len = len - offset;
    if (copy_len > size) copy_len = size;
    
    memcpy(buf, content + offset, copy_len);
    return copy_len;
}

// 读取/proc/mounts
static int read_proc_mounts(char* buf, int size, int offset)
{
    char content[256];
    strcpy(content, "/dev/vda1 / ext4 rw,relatime 0 0\n");
    strcat(content, "proc /proc proc rw,nosuid,nodev,noexec,relatime 0 0\n");
    strcat(content, "devtmpfs /dev devtmpfs rw,nosuid,size=64m,mode=755 0 0\n");
    
    int len = strlen(content);
    if (offset >= len) return 0;
    
    int copy_len = len - offset;
    if (copy_len > size) copy_len = size;
    
    memcpy(buf, content + offset, copy_len);
    return copy_len;
}

// 读取/etc/localtime (简化实现，返回UTC+8)
static int read_etc_localtime(char* buf, int size, int offset)
{
    char content[64];
    strcpy(content, "CST-8\n");
    
    int len = strlen(content);
    if (offset >= len) return 0;
    
    int copy_len = len - offset;
    if (copy_len > size) copy_len = size;
    
    memcpy(buf, content + offset, copy_len);
    return copy_len;
}

// 读取/etc/adjtime
static int read_etc_adjtime(char* buf, int size, int offset)
{
    char content[128];
    strcpy(content, "0.0 0 0.0\n");
    strcat(content, "0\n");
    strcat(content, "UTC\n");
    
    int len = strlen(content);
    if (offset >= len) return 0;
    
    int copy_len = len - offset;
    if (copy_len > size) copy_len = size;
    
    memcpy(buf, content + offset, copy_len);
    return copy_len;
}

// 读取/etc/passwd
static int read_etc_passwd(char* buf, int size, int offset)
{
    char content[256];
    strcpy(content, "root:x:0:0:root:/root:/bin/sh\n");
    strcat(content, "daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin\n");
    strcat(content, "bin:x:2:2:bin:/bin:/usr/sbin/nologin\n");
    strcat(content, "sys:x:3:3:sys:/dev:/usr/sbin/nologin\n");
    strcat(content, "nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin\n");
    
    int len = strlen(content);
    if (offset >= len) return 0;
    
    int copy_len = len - offset;
    if (copy_len > size) copy_len = size;
    
    memcpy(buf, content + offset, copy_len);
    return copy_len;
}

// 读取/etc/group
static int read_etc_group(char* buf, int size, int offset)
{
    char content[256];
    strcpy(content, "root:x:0:\n");
    strcat(content, "daemon:x:1:\n");
    strcat(content, "bin:x:2:\n");
    strcat(content, "sys:x:3:\n");
    strcat(content, "adm:x:4:\n");
    strcat(content, "tty:x:5:\n");
    strcat(content, "disk:x:6:\n");
    strcat(content, "nogroup:x:65534:\n");
    
    int len = strlen(content);
    if (offset >= len) return 0;
    
    int copy_len = len - offset;
    if (copy_len > size) copy_len = size;
    
    memcpy(buf, content + offset, copy_len);
    return copy_len;
}

// 读取/dev/misc/rtc (返回RTC时间)
static int read_dev_rtc(char* buf, int size, int offset)
{
    if (offset > 0) return 0;
    if (size < 8) return -1;
    
    uint64 rtc_time = rtc_gettime();
    memcpy(buf, &rtc_time, 8);
    return 8;
}

// 写入/dev/misc/rtc (暂不实现)
static int write_dev_rtc(const char* buf, int size, int offset)
{
    // RTC写入暂不实现
    return -1;
}

// 虚拟文件节点表
static vnode_t vnodes[] = {
    {"/proc/meminfo",   VNODE_PROC_MEMINFO,  0444, read_proc_meminfo, NULL},
    {"/proc/mounts",    VNODE_PROC_MOUNTS,   0444, read_proc_mounts, NULL},
    {"/etc/localtime",  VNODE_ETC_LOCALTIME, 0644, read_etc_localtime, NULL},
    {"/etc/adjtime",    VNODE_ETC_ADJTIME,   0644, read_etc_adjtime, NULL},
    {"/etc/passwd",     VNODE_ETC_PASSWD,    0644, read_etc_passwd, NULL},
    {"/etc/group",      VNODE_ETC_GROUP,     0644, read_etc_group, NULL},
    {"/dev/misc/rtc",   VNODE_DEV_RTC,       0644, read_dev_rtc, write_dev_rtc},
    {"/dev/rtc",        VNODE_DEV_RTC,       0644, read_dev_rtc, write_dev_rtc},
};

void procfs_init()
{
    printf("[PROCFS] Virtual filesystem initialized\n");
}

bool procfs_is_virtual(const char* path)
{
    for (int i = 0; i < ARRAY_SIZE(vnodes); i++) {
        if (strcmp(path, vnodes[i].path) == 0) {
            return true;
        }
    }
    return false;
}

int procfs_read(const char* path, char* buf, int size, int offset)
{
    for (int i = 0; i < ARRAY_SIZE(vnodes); i++) {
        if (strcmp(path, vnodes[i].path) == 0) {
            if (vnodes[i].read) {
                return vnodes[i].read(buf, size, offset);
            }
            return -1;
        }
    }
    return -1;
}

int procfs_write(const char* path, const char* buf, int size, int offset)
{
    for (int i = 0; i < ARRAY_SIZE(vnodes); i++) {
        if (strcmp(path, vnodes[i].path) == 0) {
            if (vnodes[i].write) {
                return vnodes[i].write(buf, size, offset);
            }
            return -1;
        }
    }
    return -1;
}

int procfs_stat(const char* path, file_stat_t* stat)
{
    for (int i = 0; i < ARRAY_SIZE(vnodes); i++) {
        if (strcmp(path, vnodes[i].path) == 0) {
            memset(stat, 0, sizeof(file_stat_t));
            stat->st_mode = vnodes[i].mode;
            stat->st_nlink = 1;
            stat->st_uid = 0;
            stat->st_gid = 0;
            stat->st_size = 4096;  // 虚拟文件大小
            stat->st_blksize = 512;
            stat->st_blocks = 8;
            
            timespec_t now = timer_get_ts(0);
            stat->st_atim = now;
            stat->st_mtim = now;
            stat->st_ctim = now;
            return 0;
        }
    }
    return -1;
}

#ifndef __PROCFS_H__
#define __PROCFS_H__

#include "common.h"
#include "fs/base_stat.h"

// 虚拟文件系统节点类型
typedef enum {
    VNODE_PROC_MEMINFO,
    VNODE_PROC_MOUNTS,
    VNODE_ETC_LOCALTIME,
    VNODE_ETC_ADJTIME,
    VNODE_ETC_PASSWD,
    VNODE_ETC_GROUP,
    VNODE_DEV_RTC,
    VNODE_MAX
} vnode_type_t;

// 虚拟文件节点
typedef struct vnode {
    const char* path;
    vnode_type_t type;
    uint32 mode;
    int (*read)(char* buf, int size, int offset);
    int (*write)(const char* buf, int size, int offset);
} vnode_t;

// 初始化虚拟文件系统
void procfs_init();

// 检查路径是否为虚拟文件
bool procfs_is_virtual(const char* path);

// 读取虚拟文件
int procfs_read(const char* path, char* buf, int size, int offset);

// 写入虚拟文件
int procfs_write(const char* path, const char* buf, int size, int offset);

// 获取虚拟文件状态
int procfs_stat(const char* path, file_stat_t* stat);

#endif

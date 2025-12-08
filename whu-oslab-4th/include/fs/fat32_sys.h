#ifndef __FAT32_SYS_H__
#define __FAT32_SYS_H__

#include "common.h"

void   fat32_sys_init();

uint64 fat32_sys_getcwd(uint64 dst, int size); // 获取当前目录
uint64 fat32_sys_chdir(char* path); // 改变当前目录
uint64 fat32_sys_openat(int fd, char* path, int flags, uint16 mode); // 文件打开（或创建）
uint64 fat32_sys_close(int fd); // 文件关闭
uint64 fat32_sys_read(int fd, uint64 dst, int len); // 文件读取
uint64 fat32_sys_write(int fd, uint64 src, int len); // 文件写入
uint64 fat32_sys_pipe2(uint64 dst, int flags); // 管道创建
uint64 fat32_sys_dup(int fd); // 文件描述符复制
uint64 fat32_sys_dup2(int oldfd, int newfd); // 指定newfd的文件描述符复制
uint64 fat32_sys_getdents64(int fd, uint64 dst, int len); // 获取当前目录下的所有目录项
uint64 fat32_sys_mkdirat(int fd, char* path, uint16 mode); // 创建新的目录
uint64 fat32_sys_linkat(int oldfd, char* oldpath, int newfd, char* newpath, int flags); // 链接建立
uint64 fat32_sys_unlinkat(int parfd, char* path, int flags); // 链接断开
uint64 fat32_sys_fstat(int fd, uint64 dst); // 文件状态

#endif
#ifndef __EXT4_SYS_H__
#define __EXT4_SYS_H__

#include "common.h"

void   ext4_sys_init();

uint64 ext4_sys_getcwd(uint64 dst, int size); // 获取当前目录
uint64 ext4_sys_getdents64(int fd, uint64 dst, int len); // 获取当前目录下的所有目录项
uint64 ext4_sys_mkdirat(int fd, char* path, uint16 mode); // 创建新的目录
uint64 ext4_sys_chdir(char* path); // 改变当前目录
uint64 ext4_sys_openat(int fd, char* path, int flags, uint16 mode); // 文件打开（或创建）
uint64 ext4_sys_close(int fd); // 文件关闭
uint64 ext4_sys_lseek(int fd, int64 offset, int whence);
uint64 ext4_sys_read(int fd, uint64 dst, int len); // 文件读取
uint64 ext4_sys_write(int fd, uint64 src, int len); // 文件写入
uint64 ext4_sys_readv(int fd, uint64 iov_addr, int iov_cnt);
uint64 ext4_sys_writev(int fd, uint64 iov_addr, int iov_cnt);
uint64 ext4_sys_pread64(int fd, uint64 dst, uint64 len, int64 offset); // 文件读取
uint64 ext4_sys_pwrite64(int fd, uint64 src, uint64 len, int64 offset); // 文件写入
uint64 ext4_sys_pipe2(uint64 dst, int flags); // 管道创建
uint64 ext4_sys_dup(int fd); // 文件描述符复制
uint64 ext4_sys_dup2(int oldfd, int newfd); // 指定newfd的文件描述符复制
uint64 ext4_sys_linkat(int oldfd, char* oldpath, int newfd, char* newpath, int flags); // 链接建立
uint64 ext4_sys_unlinkat(int parfd, char* path, int flags); // 链接断开
uint64 ext4_sys_fstat(int fd, uint64 dst); // 文件状态
uint64 ext4_sys_fstatat(int fd, char* path, uint64 addr_stat, int flags);
uint64 ext4_sys_faccessat(int fd, char* path, int mode, int flags);
uint64 ext4_sys_statfs(char* path, uint64 addr_stat);
uint64 ext4_sys_fcntl(int fd, int cmd, int arg);
uint64 ext4_sys_sendfile(int outfd, int infd, uint64 addr_offset, int len);
uint64 ext4_sys_utimensat(int fd, char* path, uint64 addr_ts, int flags);
uint64 ext4_sys_renameat2(int oldfd, char* oldpath, int newfd, char* newpath, int flags);
uint64 ext4_sys_ppoll(uint64 addr_fds, int nfds, uint64 addr_ts, uint64 addr_sigmask);

#endif
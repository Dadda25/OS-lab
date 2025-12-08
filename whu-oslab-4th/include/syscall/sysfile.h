#ifndef __SYSFILE_H__
#define __SYSFILE_H__

#include "common.h"

#define FD_CWD -100

typedef struct {
    uint8  fs_type;  // 文件系统类型 0->fat32 1->ext4
    uint64 (*fs_getcwd)(uint64 dst, int size); // 获取当前目录
    uint64 (*fs_getdents64)(int fd, uint64 dst, int len); // 获取当前目录下的所有目录项
    uint64 (*fs_mkdirat)(int fd, char* path, uint16 mode); // 创建新的目录
    uint64 (*fs_chdir)(char* path); // 改变当前目录
    uint64 (*fs_openat)(int fd, char* path, int flags, uint16 mode); // 文件打开（或创建）
    uint64 (*fs_close)(int fd); // 文件关闭
    uint64 (*fs_lseek)(int fd, int64 offset, int whence);
    uint64 (*fs_read)(int fd, uint64 dst, int len); // 文件读取
    uint64 (*fs_write)(int fd, uint64 dst, int len); // 文件写入
    uint64 (*fs_readv)(int fd, uint64 iov_addr, int iov_cnt); // 文件读取
    uint64 (*fs_writev)(int fd, uint64 iov_addr, int iov_cnt); // 文件写入
    uint64 (*fs_pread64)(int fd, uint64 dst, uint64 count, int64 offset);
    uint64 (*fs_pwrite64)(int fd, uint64 src, uint64 count, int64 offset);
    uint64 (*fs_pipe2)(uint64 dst, int flags); // 管道创建
    uint64 (*fs_dup)(int fd); // 文件描述符复制
    uint64 (*fs_dup2)(int oldfd, int newfd); // 指定newfd的文件描述符复制
    uint64 (*fs_linkat)(int oldfd, char* oldpath, int newfd, char* newpath, int flags); // 链接建立
    uint64 (*fs_unlinkat)(int parfd, char* path, int flags); // 链接断开
    uint64 (*fs_fstat)(int fd, uint64 dst); // 文件状态
    uint64 (*fs_fstatat)(int fd, char* path, uint64 addr_stat, int flags);
    uint64 (*fs_fcntl)(int fd, int cmd, int arg);
    uint64 (*fs_faccessat)(int fd, char* path, int mode, int flags);
    uint64 (*fs_sendfile)(int outfd, int infd, uint64 addr_offset, int len);
    uint64 (*fs_utimensat)(int fd, char* path, uint64 addr_ts, int flags);
    uint64 (*fs_ppoll)(uint64 addr_fds, int nfds, uint64 addr_ts, uint64 addr_sigmask);
    uint64 (*fs_renameat2)(int oldfd, char* oldpath, int newfd, char* newpath, int flags);
    uint64 (*fs_statfs)(char* path, uint64 addr_stat);
} FS_OP_t;

uint64 sys_getcwd();
uint64 sys_getdents64();
uint64 sys_mkdirat();
uint64 sys_chdir();
uint64 sys_openat();
uint64 sys_close();
uint64 sys_lseek();
uint64 sys_read();
uint64 sys_write();
uint64 sys_readv();
uint64 sys_writev();
uint64 sys_pread64();
uint64 sys_pwrite64();
uint64 sys_pipe2();
uint64 sys_dup();
uint64 sys_dup2();
uint64 sys_linkat();
uint64 sys_unlinkat();
uint64 sys_mount();
uint64 sys_umount2();
uint64 sys_fstat();
uint64 sys_fstatat();
uint64 sys_faccessat();
uint64 sys_statfs();
uint64 sys_utimensat();
uint64 sys_renameat2();
uint64 sys_sendfile();
uint64 sys_fcntl();
uint64 sys_ioctl();
uint64 sys_ppoll();

#endif
#include "syscall/sysfile.h"
#include "syscall/syscall.h"
#include "fs/procfs.h"
#include "mem/pmem.h"
#include "mem/vmem.h"
#include "proc/proc.h"
#include "proc/cpu.h"
#include "lib/str.h"

// 文件系统操作集合
FS_OP_t FS_OP;

// 虚拟文件描述符管理
#define VFILE_FD_BASE 1000
#define MAX_VFILES 32

typedef struct {
    bool in_use;
    char path[PATH_LEN];
    int offset;
} vfile_t;

static vfile_t vfiles[MAX_VFILES];

// 分配虚拟文件描述符
static int alloc_vfd(const char* path)
{
    for (int i = 0; i < MAX_VFILES; i++) {
        if (!vfiles[i].in_use) {
            vfiles[i].in_use = true;
            strncpy(vfiles[i].path, path, PATH_LEN - 1);
            vfiles[i].path[PATH_LEN - 1] = '\0';
            vfiles[i].offset = 0;
            return VFILE_FD_BASE + i;
        }
    }
    return -1;
}

// 释放虚拟文件描述符
static void free_vfd(int fd)
{
    int idx = fd - VFILE_FD_BASE;
    if (idx >= 0 && idx < MAX_VFILES) {
        vfiles[idx].in_use = false;
    }
}

// 检查是否为虚拟文件描述符
static bool is_vfd(int fd)
{
    return fd >= VFILE_FD_BASE && fd < VFILE_FD_BASE + MAX_VFILES;
}

// 获取虚拟文件信息
static vfile_t* get_vfile(int fd)
{
    int idx = fd - VFILE_FD_BASE;
    if (idx >= 0 && idx < MAX_VFILES && vfiles[idx].in_use) {
        return &vfiles[idx];
    }
    return NULL;
}

// 获取当前工作目录
// char* buf 用于存放工作目录的字符串
// int size  buf的大小
// 成功返回buf的指针, 失败返回NULL 
uint64 sys_getcwd()
{
    uint64 dst;
    int size;
    arg_addr(0, &dst);
    arg_int(1, &size);

    return FS_OP.fs_getcwd(dst, size);
}

// 切换工作目录
// char* path 要切换到的目录
// 成功返回0 失败返回-1
uint64 sys_chdir()
{
    char path[PATH_LEN];
    if(arg_str(0, path, PATH_LEN) < 0) 
        return -1;

    return FS_OP.fs_chdir(path);
}

// 按条目获取目录的内容
// int fd  要读取的目录名
// dirent_t* buf  保存目录信息的缓冲区
// int len  缓存区的大小
// 成功返回读取的字节数 失败返回-1 
uint64 sys_getdents64()
{

    int fd, size;
    uint64 dst;
    arg_int(0, &fd);
    if(fd != FD_CWD && (fd < 0 || fd >= NOFILE))
        return -1;
    arg_addr(1, &dst);
    arg_int(2, &size);

    return FS_OP.fs_getdents64(fd, dst, size);
}

// 创建目录文件
// int dirfd       要创建的目录文件所在目录的描述符
// char* path      要创建的目录文件的路径(规则见sys_read)
// mode_t mode     文件所有权(忽略)
// 成功返回0 失败返回-1
uint64 sys_mkdirat()
{
    int fd, mode;
    char path[PATH_LEN];
    path[PATH_LEN - 1] = '\0';

    arg_int(0, &fd);
    if(fd != FD_CWD && (fd < 0 || fd >= NOFILE))
        return -1;    
    if(arg_str(1, path, PATH_LEN - 1) < 0)
        return -1;
    arg_int(3, &mode);

    return FS_OP.fs_mkdirat(fd, path, (uint16)mode);
}

// 打开或创建一个文件
// int fd 文件所在目录的描述符
// char* path 目标文件的路径
// int flags 访问模式
// mode_t mode 文件所有权(暂时忽略)
// 1. 路径是绝对路径则忽略fd
// 2. 路径是相对路径且fd=-100   说明相对于cwd
// 3. 路径是相对路径且fd是合法值 说明相对于fd指向的路径
// 成功返回打开或创建的文件的描述符fd 失败返回-1 
uint64 sys_openat()
{
    int fd, flags, mode;   
    char path[PATH_LEN];
    path[PATH_LEN - 1] = '\0';

    arg_int(0, &fd);
    if(fd != FD_CWD && (fd < 0 || fd >= NOFILE))
        return -1;    
    if(arg_str(1, path, PATH_LEN - 1) < 0)
        return -1;
    arg_int(2, &flags);
    arg_int(3, &mode);
    
    // 检查是否为虚拟文件
    if (procfs_is_virtual(path)) {
        // 分配虚拟文件描述符
        return alloc_vfd(path);
    }
    
    return FS_OP.fs_openat(fd, path, flags, (uint16)mode);
}

// 关闭一个文件
// int fd 需要关闭的文件的描述符
// 成功返回0 失败返回-1
uint64 sys_close()
{
    int fd;
    arg_int(0, &fd);
    if(fd < 0 || fd >= NOFILE) {
        // 检查是否为虚拟文件描述符
        if (is_vfd(fd)) {
            free_vfd(fd);
            return 0;
        }
        return -1;
    }

    return FS_OP.fs_close(fd);
}

// 文件指针移动
// int fd
// uin32 offset
// int whence
uint64 sys_lseek()
{
    int fd, whence;
    uint64 offset;

    arg_int(0, &fd);
    arg_addr(1, &offset);
    arg_int(2, &whence);

    if(fd < 0 || fd >= NOFILE)
        return -1;

    return FS_OP.fs_lseek(fd, offset, whence);
}

// 文件读取
// int fd    目标文件描述符
// char* buf 读取内容的缓存区
// int count 读取的字节数
// 成功返回读取字节数 失败返回-1
uint64 sys_read()
{
    int fd, len;
    uint64 dst;

    arg_int(0, &fd);
    if(fd < 0 || fd >= NOFILE) {
        // 检查是否为虚拟文件描述符
        if (is_vfd(fd)) {
            vfile_t* vf = get_vfile(fd);
            if (!vf) return -1;
            
            arg_addr(1, &dst);
            arg_int(2, &len);
            
            char* buf = (char*)pmem_alloc_pages(1, true);
            if (!buf) return -1;
            
            int ret = procfs_read(vf->path, buf, len, vf->offset);
            if (ret > 0) {
                if (uvm_copyout(myproc()->pagetable, dst, (uint64)buf, ret) < 0) {
                    ret = -1;
                } else {
                    vf->offset += ret;
                }
            }
            
            pmem_free_pages(buf, 1, true);
            return ret;
        }
        return -1;
    }
    arg_addr(1, &dst);
    arg_int(2, &len);

    return FS_OP.fs_read(fd, dst, len);    
}

// 文件写入
// int fd     目标文件描述符
// char* buf  写入的内容
// int count  写入的字节数
// 成功返回写入的字节数 失败返回-1
uint64 sys_write()
{
    int fd, len;
    uint64 src;

    arg_int(0, &fd);
    if(fd < 0 || fd >= NOFILE) {
        // 检查是否为虚拟文件描述符
        if (is_vfd(fd)) {
            vfile_t* vf = get_vfile(fd);
            if (!vf) return -1;
            
            arg_addr(1, &src);
            arg_int(2, &len);
            
            char* buf = (char*)pmem_alloc_pages(1, true);
            if (!buf) return -1;
            
            if (uvm_copyin(myproc()->pagetable, (uint64)buf, src, len) < 0) {
                pmem_free_pages(buf, 1, true);
                return -1;
            }
            
            int ret = procfs_write(vf->path, buf, len, vf->offset);
            if (ret > 0) {
                vf->offset += ret;
            }
            
            pmem_free_pages(buf, 1, true);
            return ret;
        }
        return -1;
    }
    arg_addr(1, &src);
    arg_int(2, &len);

    return FS_OP.fs_write(fd, src, len);
}


// 多组缓冲区的文件读取
// int fd
// iovec_t* iov
// int iovcnt
uint64 sys_readv()
{
    int fd, iov_cnt;
    uint64 iov_addr;

    arg_int(0, &fd);
    arg_addr(1, &iov_addr);
    arg_int(2, &iov_cnt);
    if(fd < 0 || fd >= NOFILE)
        return -1;
    return FS_OP.fs_readv(fd, iov_addr, iov_cnt);
}

// 多组缓冲区的文件写入
// int fd
// iovec_t* iov
// int iovcnt
uint64 sys_writev()
{
    int fd, iov_cnt;
    uint64 iov_addr;

    arg_int(0, &fd);
    arg_addr(1, &iov_addr);
    arg_int(2, &iov_cnt);
    if(fd < 0 || fd >= NOFILE)
        return -1;
    return FS_OP.fs_writev(fd, iov_addr, iov_cnt);
}

// int fd
// char* buf
// uint64 count
// int64 offset
// 成功返回字节数 失败返回-1
uint64 sys_pread64()
{
    int fd;
    uint64 addr_buf, count, offset;

    arg_int(0, &fd);
    arg_addr(1, &addr_buf);
    arg_addr(2, &count);
    arg_addr(3, &offset);

    if(fd < 0 || fd >= NOFILE)
        return -1;

    return FS_OP.fs_pread64(fd, addr_buf, count, offset);
}

// int fd
// char* buf
// uint64 count
// int64 offset
// 成功返回字节数 失败返回-1
uint64 sys_pwrite64()
{
    int fd;
    uint64 addr_buf, count, offset;

    arg_int(0, &fd);
    arg_addr(1, &addr_buf);
    arg_addr(2, &count);
    arg_addr(3, &offset);

    if(fd < 0 || fd >= NOFILE)
        return -1;

    return FS_OP.fs_pwrite64(fd, addr_buf, count, offset);
}

// 创建一个管道
// int fd[2] 用于保存两个文件描述符
// int flags
// 其中fd[0]为读端口 fd[1]为写端口
// 成功返回0 失败返回-1
uint64 sys_pipe2()
{
    uint64 dst;
    int flags;
    arg_addr(0, &dst);
    arg_int(1, &flags);

    return FS_OP.fs_pipe2(dst, flags);
}

// 复制文件描述符
// int fd 被复制的文件描述符
// 成功返回新复制的fd 失败返回-1
uint64 sys_dup(void)
{
    int fd;
    arg_int(0, &fd);
    if(fd < 0 || fd >= NOFILE)
        return -1;

    return FS_OP.fs_dup(fd);
}

// 复制文件描述符,指定了新的文件描述符
// int old 被复制的文件描述符
// int new 指定的新的文件描述符
// 成功返回新的描述符 失败返回-1
uint64 sys_dup2()
{
    int old_fd, new_fd;

    arg_int(0, &old_fd);
    arg_int(1, &new_fd);
    if(old_fd < 0 || old_fd >= NOFILE)
        return -1;
    if(new_fd < 0 || new_fd >= NOFILE)
        return -1;

    return FS_OP.fs_dup2(old_fd, new_fd);
}

// 创建文件的链接
// int olddirfd    被链接的文件所在目录的描述符
// char* oldpath   被链接的文件的路径(规则见sys_read)
// int newdirfd    新文件所在目录的描述符
// char* newpath   新文件的路径(规则见sys_read)
// int flags    (忽略)
// 成功返回0 失败返回-1
uint64 sys_linkat()
{
    int old_fd, new_fd, flags;
    char old_path[PATH_LEN], new_path[PATH_LEN];
    old_path[PATH_LEN - 1] = '\0';
    new_path[PATH_LEN - 1] = '\0';

    arg_int(0, &old_fd);
    arg_int(2, &new_fd);
    if(old_fd != FD_CWD && (old_fd < 0 || old_fd >= NOFILE))
        return -1;
    if(new_fd != FD_CWD && (new_fd < 0 || new_fd >= NOFILE))
        return -1;
    if(arg_str(1, old_path, PATH_LEN - 1) < 0)
        return -1;
    if(arg_str(3, new_path, PATH_LEN - 1) < 0)
        return -1;
    arg_int(4, &flags);

    return FS_OP.fs_linkat(old_fd, old_path, new_fd, new_path, flags);
}

// 删除文件的链接
// int dirfd       要删除的链接所在目录的描述符
// char* path      要删除的链接的路径(规则见sys_read)
// int flags       (忽略)
// 成功返回0 失败返回-1
uint64 sys_unlinkat()
{
    int fd, flags;
    char path[PATH_LEN];
    path[PATH_LEN - 1] = '\0';

    arg_int(0, &fd);
    if(fd != FD_CWD && (fd < 0 || fd >= NOFILE))
        return -1;
    if(arg_str(1, path, PATH_LEN - 1) < 0)
        return -1;
    arg_int(2, &flags);

    return FS_OP.fs_unlinkat(fd, path, flags);
}

// 文件系统挂载 (未实现)
// const char* source   挂载设备
// const char* target   挂载点
// const char* fstype   文件系统类型
// uint64 mountflags    挂载参数
// const void* data     字符串参数
// 成功返回0 失败返回-1
uint64 sys_mount()
{
    return 0;
}

// 文件系统卸载 (未实现)
// const char* target   挂载点
// int umountflags      卸载参数
// 成功返回0 失败返回-1
uint64 sys_umount2()
{
    return 0;
}

// 获取文件状态
// int fd            目标文件
// file_stat_t* kst  接受文件状态的指针
// 成功返回0 失败返回-1
uint64 sys_fstat()
{   
    int fd;
    uint64 dst;

    arg_int(0, &fd);
    if(fd < 0 || fd >= NOFILE) {
        // 检查是否为虚拟文件描述符
        if (is_vfd(fd)) {
            vfile_t* vf = get_vfile(fd);
            if (!vf) return -1;
            
            arg_addr(1, &dst);
            
            file_stat_t stat;
            if (procfs_stat(vf->path, &stat) < 0) {
                return -1;
            }
            
            return uvm_copyout(myproc()->pagetable, dst, (uint64)&stat, sizeof(stat));
        }
        return -1;
    }
    arg_addr(1, &dst);

    return FS_OP.fs_fstat(fd, dst);
}

// int dirfd
// char* path
// file_stat_t* kst
// int flags
// 成功返回0 失败返回-1
uint64 sys_fstatat()
{
    int dirfd, flags;
    uint64 addr_stat;
    char path[PATH_LEN];

    arg_int(0, &dirfd);
    if((dirfd != FD_CWD) && (dirfd < 0 || dirfd >= NFILE))
        return -1;
    if(arg_str(1, path, PATH_LEN) < 0)
        return -1;
    arg_addr(2, &addr_stat);
    arg_int(3, &flags);

    return FS_OP.fs_fstatat(dirfd, path, addr_stat, flags);
}

// int dirfd
// char* path
// int mode      测试对象
// int flags
// 成功返回0 失败返回-1
uint64 sys_faccessat()
{
    int dirfd, mode, flags;
    char path[PATH_LEN];

    arg_int(0, &dirfd);
    if((dirfd != FD_CWD) && (dirfd < 0 || dirfd >= NFILE))
        return -1;
    if(arg_str(1, path, PATH_LEN) < 0)
        return -1;
    arg_int(2, &mode);
    arg_int(3, &flags);

    return FS_OP.fs_faccessat(dirfd, path, mode, flags);    
}

// 获取文件系统信息
// char* path
// struct statfs* p 
uint64 sys_statfs()
{
    char path[PATH_LEN];
    uint64 addr_stat;
    arg_str(0, path, PATH_LEN);
    arg_addr(1, &addr_stat);
    return FS_OP.fs_statfs(path, addr_stat);
}

// 高效地将数据从一个文件复制到另一个文件
// int out_fd
// int in_fd
// uint32* addr_off
// uint32 len
// 成功则返回转移的字节数, 失败则返回-1
uint64 sys_sendfile()
{
    int outfd, infd, len;
    uint64 addr_off;

    arg_int(0, &outfd);
    arg_int(1, &infd);
    if(outfd < 0 || outfd >= NOFILE)
        return -1;
    if(infd < 0 || infd >= NOFILE)
        return -1;    
    arg_addr(2, &addr_off);
    arg_int(3, &len);

    return FS_OP.fs_sendfile(outfd, infd, addr_off, len);
}

// 更新文件时间戳
// int dirfd
// char* path
// timespec_t ts[2]
// int flags
// 成功返回0 如果文件不存在则返回-1
uint64 sys_utimensat()
{
    int fd, flags;
    uint64 addr_ts;
    char path[PATH_LEN];
    path[PATH_LEN - 1] = '\0';

    arg_int(0, &fd);
    if(fd != FD_CWD && (fd < 0 || fd >= NOFILE))
        return -1;    
    if(arg_str(1, path, PATH_LEN - 1) < 0)
        return -1;
    arg_addr(2, &addr_ts);
    arg_int(3, &flags);
    
    return FS_OP.fs_utimensat(fd, path, addr_ts, flags);
    
}

// 文件重命名
// int olddirfd
// char* oldpath
// int newdirfd
// char* newpath
// int flags
uint64 sys_renameat2()
{
    int old_fd, new_fd, flags;
    char old_path[PATH_LEN], new_path[PATH_LEN];
    old_path[PATH_LEN - 1] = '\0';
    new_path[PATH_LEN - 1] = '\0';

    arg_int(0, &old_fd);
    arg_int(2, &new_fd);
    if(old_fd != FD_CWD && (old_fd < 0 || old_fd >= NOFILE))
        return -1;
    if(new_fd != FD_CWD && (new_fd < 0 || new_fd >= NOFILE))
        return -1;
    if(arg_str(1, old_path, PATH_LEN - 1) < 0)
        return -1;
    if(arg_str(3, new_path, PATH_LEN - 1) < 0)
        return -1;
    arg_int(4, &flags);

    return FS_OP.fs_renameat2(old_fd, old_path, new_fd, new_path, flags);
}

// 文件控制相关
// int fd    文件描述符
// int cmd   命令
// int arg   参数
// 返回值依赖具体情况而定
uint64 sys_fcntl()
{
    int fd, cmd, arg;
    arg_int(0, &fd);
    arg_int(1, &cmd);
    arg_int(2, &arg);
    return FS_OP.fs_fcntl(fd, cmd, arg);
}

// I/O控制相关
uint64 sys_ioctl()
{
    return 0;
}

// 等待其中一个文件描述符就绪
// struct ppoll *fds
// int nfds
// timespec_t* ts
// sigset_t* sigmask
uint64 sys_ppoll()
{
    uint64 addr_fds, addr_ts, addr_sigmask;
    int nfds;

    arg_addr(0, &addr_fds);
    arg_int(1, &nfds);
    arg_addr(2, &addr_ts);
    arg_addr(2, &addr_sigmask);

    return FS_OP.fs_ppoll(addr_fds, nfds, addr_ts, addr_sigmask);
}
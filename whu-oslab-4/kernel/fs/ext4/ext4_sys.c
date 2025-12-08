#include "syscall/sysfile.h"
#include "fs/ext4.h"
#include "fs/ext4_sys.h"
#include "fs/ext4_file.h"
#include "fs/ext4_dir.h"
#include "fs/ext4_inode.h"
#include "fs/ext4_pipe.h"
#include "fs/base_stat.h"

#include "dev/console.h"
#include "proc/cpu.h"
#include "mem/vmem.h"
#include "mem/pmem.h"
#include "lib/print.h"
#include "lib/str.h"

extern FS_OP_t FS_OP;
extern ext4_superblock_t ext4_sb;

// 在当前进程的打开文件表中找到一个空闲的fd
// 返回获得的fd
static int fd_alloc(ext4_file_t* file)
{
    proc_t* p = myproc();
    for(int i = 0; i < NOFILE; i++) {
        if(p->ext4_ofile[i] == NULL) {
            p->ext4_ofile[i] = file;
            return i;
        }
    }
    panic("fd_alloc");
    return -1;
}

// 成功返回0 失败返回-1
static int get_refer(int fd, char first, ext4_inode_t** refer)
{
    ext4_file_t* file;
    *refer = NULL;
    if(first != '/' && fd != FD_CWD) { // fd有效
        file = myproc()->ext4_ofile[fd];
        if(file == NULL || file->ip == NULL) return -1;
        *refer = file->ip;
    }
    return 0;
}

// 权限检查(目前只检查读写权限)
static bool check_flags(int flags, uint16 mode)
{
    // printf("check_flags: flags = %x, mode = %x\n", flags, mode);
    return true; // @TODO 这里默认所有文件都是具有任何权限的，可能会出问题
    bool ok = true;
    if(flags & FLAGS_RDONLY) { // 只读
        if(!(mode & (~IMODE_MASK) & IMODE_A_READ))
            ok = false;    
    } else if(flags & FLAGS_WRONLY) { // 只写
        if(!(mode & (~IMODE_MASK) & IMODE_A_WRIT))
            ok = false;
    } else if(flags & FLAGS_RDWR) { // 既读又写
        if(!(mode & (~IMODE_MASK) & IMODE_A_READ))
            ok = false;    
        if(!(mode & (~IMODE_MASK) & IMODE_A_WRIT))
            ok = false;
    }
    return ok;
}

// ext4 系统调用注册
void ext4_sys_init()
{
    FS_OP.fs_type = 1;
    /* 目录相关操作 */
    FS_OP.fs_getcwd = ext4_sys_getcwd;
    FS_OP.fs_chdir = ext4_sys_chdir;
    FS_OP.fs_mkdirat = ext4_sys_mkdirat;
    FS_OP.fs_getdents64 = ext4_sys_getdents64;
    /* 文件打开、关闭、读写操作 */
    FS_OP.fs_lseek = ext4_sys_lseek;
    FS_OP.fs_openat = ext4_sys_openat;
    FS_OP.fs_close = ext4_sys_close;
    FS_OP.fs_read = ext4_sys_read;
    FS_OP.fs_write = ext4_sys_write;
    FS_OP.fs_readv = ext4_sys_readv;
    FS_OP.fs_writev = ext4_sys_writev;
    FS_OP.fs_pread64 = ext4_sys_pread64;
    FS_OP.fs_pwrite64 = ext4_sys_pwrite64;
    FS_OP.fs_sendfile = ext4_sys_sendfile;
    /* 其他操作 */
    FS_OP.fs_dup = ext4_sys_dup;
    FS_OP.fs_dup2 = ext4_sys_dup2;
    FS_OP.fs_pipe2 = ext4_sys_pipe2;
    FS_OP.fs_linkat = ext4_sys_linkat;
    FS_OP.fs_unlinkat = ext4_sys_unlinkat;
    /* 获取或设置文件信息 */
    FS_OP.fs_fstat = ext4_sys_fstat;
    FS_OP.fs_fstatat = ext4_sys_fstatat;
    FS_OP.fs_faccessat = ext4_sys_faccessat;
    FS_OP.fs_statfs = ext4_sys_statfs;
    FS_OP.fs_fcntl = ext4_sys_fcntl;
    FS_OP.fs_utimensat = ext4_sys_utimensat;
    FS_OP.fs_ppoll = ext4_sys_ppoll;
    FS_OP.fs_renameat2 = ext4_sys_renameat2;

    // 目录的准备工作
    assert(ext4_sys_mkdirat(-100, "./tmp", IMODE_DIR) == 0, "ext4_sys_init: 0");
    assert(ext4_sys_mkdirat(-100, "./dev", IMODE_DIR) == 0, "ext4_sys_init: 1");
    
    int fd = ext4_sys_openat(-100, "./dev/null", FLAGS_CREATE, 0x666 | IMODE_CHAR);
    assert(fd != -1, "ext4_sys_init: 2");
    ext4_sys_close(fd);

    fd = ext4_sys_openat(-100, "./dev/zero", FLAGS_CREATE, 0x444 | IMODE_CHAR);
    assert(fd != -1, "ext4_sys_init: 3");
    ext4_sys_close(fd);
}

// 获得当前工作目录的路径
// 失败返回0, 成功返回dst
uint64 ext4_sys_getcwd(uint64 dst, int size)
{
    proc_t* p = myproc();
    ext4_inode_t* ip = p->ext4_cwd;
    uint32 slen = strlen(ip->path) + 1;

    if(slen > size || uvm_copyout(p->pagetable, dst, (uint64)(ip->path), slen) < 0)
        return 0;
    else 
        return dst;
}

// 改变工作目录
// 成功返回0, 失败返回-1
uint64 ext4_sys_chdir(char* path)
{
    ext4_inode_t* ip = ext4_dir_path_to_inode(path, NULL);
    if(ip == NULL) return -1;
    proc_t* p = myproc();

    ext4_inode_lock(ip);
    if(ip->mode & IMODE_DIR) {
        ext4_inode_unlock(ip);
        ext4_inode_put(p->ext4_cwd);
        p->ext4_cwd = ip;
        return 0;
    }
    ext4_inode_unlockput(ip);
    return -1;
}

typedef struct user_dirent {
    uint64 inum;
    uint64 offset;
    uint16 len;
    uint8  file_type;
    char   name[EXT4_NAME_LEN];
} user_dirent_t;

// 获取目录项信息
// 成功返回读取的字节数, 失败返回-1
uint64 ext4_sys_getdents64(int fd, uint64 dst, int len)
{
    proc_t* p = myproc();
    ext4_file_t* f = p->ext4_ofile[fd];
    ext4_dirent_t de;
    user_dirent_t ude;
    uint32 u_totol_len = 0, off;
    uint16 u_real_len = 0;

    if(f == NULL || f->file_type != TYPE_DIRECTORY) 
        return -1;

    // 这里认为所有的有效目录项都放在第一个block里面
    ext4_inode_lock(f->ip);
    for(off = f->off; off < BLOCK_SIZE - 12; off += de.len) {
        ext4_dir_next(f->ip, off, &de);
        if(de.inum == 0) continue;

        u_real_len = 20 + de.name_len;
        if(u_totol_len + u_real_len > len) break;
        u_totol_len += u_real_len;

        ude.inum = de.inum;
        ude.len = u_real_len;
        ude.offset = u_totol_len;
        ude.file_type = de.file_type;
        memmove(ude.name, de.name, de.name_len);
        ude.name[de.name_len] = '\0';
        if(uvm_copyout(p->pagetable, dst, (uint64)&ude, ude.len) < 0) {
            ext4_inode_unlock(f->ip);
            return -1;
        }
        dst += u_real_len;
    }
    f->off = off;
    ext4_inode_unlock(f->ip);
    return (uint64)u_totol_len;
}

// 创建目录 (暂时忽略mode)
// 成功返回0 失败返回-1
uint64 ext4_sys_mkdirat(int fd, char* path, uint16 mode)
{
    ext4_inode_t *refer, *pip, *ip;
    char name[EXT4_NAME_LEN];

    if(get_refer(fd, path[0], &refer) < 0)
        return -1;
    
    pip = ext4_dir_path_to_pinode(path, name, refer);
    if(pip == NULL) return -1;

    ext4_inode_lock(pip);
    ip = ext4_inode_create(pip, name, (uint16)(0x666 | IMODE_DIR));
    ext4_dir_create(pip, name, ip->inum, TYPE_DIRECTORY);
    ext4_dir_init(ip);
    ext4_inode_unlock(pip);

    return 0;
}

// 打开或创建文件
// 成功返回fd 失败返回-1
uint64 ext4_sys_openat(int fd, char* path, int flags, uint16 mode)
{
    // printf("ext4_sys_openat: fd=%d, path=%s, flags=%d, mode=%x\n", fd, path, flags, mode);
    ext4_inode_t *pip, *ip, *refer;
    ext4_file_t* file;

    if(get_refer(fd, path[0], &refer) < 0) {
        printf("ext4_sys_openat: get_refer failed, fd=%d, path=%s\n", fd, path);
        return -1;
    }

    char name[EXT4_NAME_LEN];
    pip = ext4_dir_path_to_pinode(path, name, refer);
    if(pip == NULL) {
        printf("ext4_sys_openat: ext4_dir_path_to_pinode failed, fd=%d, path=%s\n", fd, path);
        return -1;
    }

    ext4_inode_lock(pip);
    ip = ext4_dir_pinode_to_inode(pip, name);

    // printf("ext4_sys_openat: pip=%p, ip=%p, name=%s\n", pip, ip, name);

    if(ip == NULL) {               // 1-文件不存在
        if(flags & FLAGS_CREATE) { // 1.1-创建文件
            if((mode & IMODE_MASK) == IMODE_CHAR) {
                ip = ext4_inode_create(pip, name, mode);
                ext4_inode_lock(ip);
                ext4_dir_create(pip, ip->name, ip->inum, TYPE_CHARDEV);
                ext4_inode_unlockput(pip);                
            } else {
                ip = ext4_inode_create(pip, name, mode | IMODE_FILE);
                ext4_inode_lock(ip);
                ext4_dir_create(pip, ip->name, ip->inum, TYPE_REGULAR);
                ext4_inode_unlockput(pip);
            }
        } else {                   // 1.2-打开失败
            ext4_inode_unlockput(pip);
            return -1;
        }
    } else {                       // 2-文件存在
        ext4_inode_unlockput(pip);
        ext4_inode_lock(ip);
        if(check_flags(flags, ip->mode) == false) {
            ext4_inode_unlockput(ip);
            printf("ext4_sys_openat: check_flags failed, fd=%d, path=%s, flags=%d, mode=%x\n", fd, path, flags, mode);
            return -1;
        }
    }
    /* 到此获得了一个上了锁的inode */
    
    file = ext4_file_alloc();
    
    file->ip = ip;
    file->off = 0;
    file->flags_high = 0;
    file->flags_low  = 0;
    file->file_type = mode_to_type(ip->mode);
    file->oflags = flags;
    
    if(strncmp(file->ip->name, "null", 5) == 0)
        file->major = DEVNULL;
    if(strncmp(file->ip->name, "zero", 5) == 0)
        file->major = DEVZERO;

    ext4_inode_unlock(ip);
    
    return fd_alloc(file);
}

// 关闭文件
// 成功返回0 失败返回-1
uint64 ext4_sys_close(int fd)
{
    proc_t* p = myproc();
    ext4_file_t* file;

    file = p->ext4_ofile[fd];
    if(file == NULL) return -1;

    ext4_file_close(file);
    p->ext4_ofile[fd] = NULL;

    return 0;
}

// 用于lssek
#define SEEK_SET 0  /* Seek from beginning of file.  */
#define SEEK_CUR 1  /* Seek from current position.  */
#define SEEK_END 2  /* Seek from end of file.  */
#define SEEK_DATA 3 /* Seek to next data.  */
#define SEEK_HOLE 4 /* Seek to next hole.  */

// 文件指针移动
// 成功返回new_offset 失败返回-1
uint64 ext4_sys_lseek(int fd, int64 offset, int whence)
{
    proc_t* p = myproc();
    ext4_file_t* file = p->ext4_ofile[fd];
    int64 new_offset = 0;

    if(file == NULL)
        return -1;
    
    spinlock_acquire(&file->lk);
    if(whence == SEEK_SET || whence == SEEK_DATA) {
        new_offset = offset;
    } else if(whence == SEEK_CUR) {
        new_offset = offset + file->off;
    } else if(whence == SEEK_END) {
        assert(file->ip != NULL, "ext4_sys_lseek: 0");
        new_offset = file->ip->size + offset;
    } else if(whence == SEEK_HOLE) {
        assert(file->ip != NULL, "ext4_sys_lseek: 1");
        new_offset = file->ip->size;
    } else {
        panic("ext4_sys_lseek: 2");
    }
    if(new_offset < 0) {
        new_offset = -22;
    } else { 
        file->off = new_offset;
    }
    spinlock_release(&file->lk);
    return new_offset;
}

// 文件内容读取
// 成功返回读取字节数 失败返回-1
uint64 ext4_sys_read(int fd, uint64 dst, int len)
{
    ext4_file_t* file = myproc()->ext4_ofile[fd];
    if(file == NULL) return -1;
    uint64 read_len = 0;

    spinlock_acquire(&file->lk);
    read_len = ext4_file_read(file, dst, len, true);
    if(read_len > 0) 
        file->off += read_len;
    spinlock_release(&file->lk);
    return read_len;
}

// 文件内容写入
// 成功返回写入字节数 失败返回-1
uint64 ext4_sys_write(int fd, uint64 src, int len)
{
    ext4_file_t* file = myproc()->ext4_ofile[fd];
    if(file == NULL) return -1;
    uint64 write_len = 0;

    spinlock_acquire(&file->lk);
    // printf("ext4_sys_write: fd=%d, src=%p, len=%d, oflags=%x\n", fd, src, len, file->oflags);
    if(file->oflags & FLAGS_APPEND) {// 追加写
        file->off = file->ip->size; 
    } else if (fd > 2 && file->ip != NULL) { // @TODO 这里直接认为所有的打开文件都是覆写，所以无法解决追加写的情况。
        // printf("ext4_sys_write: fd=%d, file->ip->size=%d, src=%p, len=%d\n", fd, file->ip->size, src, len);
        file->ip->size = 0;
    }
    write_len = ext4_file_write(file, src, len, true);
    // printf("!!!!!!!!!!!!ext4_sys_write: file->off=%d, write_len=%d\n", file->off, write_len);
    if(write_len > 0) {
        file->off += write_len;
    }
    file->off = 0;
    spinlock_release(&file->lk);
    // if (fd != 1)
    // printf("ext4_sys_write: fd=%d, src=%p, len=%d, write_len=%d\n", fd, src, len, write_len);
    return write_len;
}

typedef struct iovec {
    uint64 start; // 起始地址
    uint64 len;   // 长度(字节)
} iovec_t;

// 多组缓冲区的文件内容读取
// 成功返回读取的字节数 失败返回-1
uint64 ext4_sys_readv(int fd, uint64 iov_addr, int iov_cnt)
{
    proc_t* p = myproc();
    ext4_file_t* file = p->ext4_ofile[fd];
    if(file == NULL) return -1;

    iovec_t iov;
    int totol_len = 0, read_len = 0;
    spinlock_acquire(&file->lk);
    for(int i = 0; i < iov_cnt; i++) {
        if(uvm_copyin(p->pagetable, (uint64)&iov, iov_addr + sizeof(iov), sizeof(iov)) < 0) {
            spinlock_release(&file->lk);
            return -1;
        }
        read_len = ext4_file_read(file, iov.start, iov.len, true);
        if(read_len >= 0) 
            file->off += read_len;
        else
            break;
        totol_len += read_len;
    }
    spinlock_release(&file->lk);
    return totol_len;
}

// 多组缓冲区的文件内容写入
// 成功返回写入的字节数 失败返回-1
uint64 ext4_sys_writev(int fd, uint64 iov_addr, int iov_cnt)
{
    proc_t* p = myproc();
    ext4_file_t* file = p->ext4_ofile[fd];
    if(file == NULL) return -1;

    iovec_t iov;
    int totol_len = 0, write_len = 0;
    spinlock_acquire(&file->lk);
    for(int i = 0; i < iov_cnt; i++) {
        if(uvm_copyin(p->pagetable, (uint64)&iov, iov_addr + i*sizeof(iov), sizeof(iov)) < 0)
            return -1;
        write_len = ext4_file_write(file, iov.start, iov.len, true);
        if(write_len >= 0) 
            file->off += write_len;
        else
            break;
        totol_len += write_len;
    }
    spinlock_release(&file->lk);
    return totol_len;
}

uint64 ext4_sys_pread64(int fd, uint64 dst, uint64 len, int64 offset)
{
    ext4_file_t* file = myproc()->ext4_ofile[fd];
    if(file == NULL) return -1;
    assert(offset >= 0, "ext4_sys_pread64");
    uint64 read_len = 0;
    uint32 tmp_offset = 0;

    spinlock_acquire(&file->lk);
    tmp_offset = file->off;
    file->off = (uint32)offset;
    read_len = ext4_file_read(file, dst, len, true);    
    file->off = tmp_offset;
    spinlock_release(&file->lk);
    return read_len;
}

uint64 ext4_sys_pwrite64(int fd, uint64 src, uint64 len, int64 offset)
{
    ext4_file_t* file = myproc()->ext4_ofile[fd];
    if(file == NULL) return -1;
    assert(offset >= 0, "ext4_sys_pwrite64");
    uint64 write_len = 0;
    uint32 tmp_offset = 0;

    spinlock_acquire(&file->lk);
    tmp_offset = file->off;
    file->off = (uint32)offset;
    write_len = ext4_file_write(file, src, len, true);    
    file->off = tmp_offset;
    spinlock_release(&file->lk);
    return write_len;
}

// 文件内容转移
// 成功返回转移的字节数 失败返回-1
uint64 ext4_sys_sendfile(int outfd, int infd, uint64 addr_offset, int len)
{
    assert(addr_offset == 0, "ext4_sys_sendfile: 0");

    proc_t* p = myproc();
    ext4_file_t* in_file = p->ext4_ofile[infd];
    ext4_file_t* out_file = p->ext4_ofile[outfd];
    if(in_file == NULL || out_file == NULL) return -1;
    
    int success_len = 0, cut_len = 0, totol_len = 0;
    int try = min(in_file->ip->size - in_file->off, len);
    uint8* buf = pmem_alloc_pages(1, true);
    assert(buf != NULL, "ext4_sys_sendfile: 1");
    
    spinlock_acquire(&in_file->lk);
    spinlock_acquire(&out_file->lk);
    while(totol_len < try) {
        cut_len = min(PAGE_SIZE, try - totol_len);

        success_len = ext4_file_read(in_file, (uint64)buf, cut_len, false);
        assert(success_len == cut_len, "ext4_sys_sendfile: 2");
        in_file->off += success_len;

        success_len = ext4_file_write(out_file, (uint64)buf, cut_len, false);
        assert(success_len == cut_len, "ext4_sys_sendfile: 3");
        out_file->off += success_len;

        totol_len += success_len;
    }
    spinlock_release(&in_file->lk);
    spinlock_release(&out_file->lk);

    pmem_free_pages(buf, 1, true);
    return try;
}

// 创建一个管道
// 成功返回0 失败返回-1
uint64 ext4_sys_pipe2(uint64 dst, int flags)
{
    proc_t* p = myproc();
    ext4_file_t *rf, *wf;
    int fd[2];

    if(ext4_pipe_alloc(&rf, &wf) < 0) 
        return -1;
    fd[0] = fd_alloc(rf);
    fd[1] = fd_alloc(wf);

    if(uvm_copyout(p->pagetable, dst, (uint64)fd, sizeof(fd)) < 0) {
        p->ext4_ofile[fd[0]] = NULL;
        p->ext4_ofile[fd[1]] = NULL;
        ext4_file_close(rf);
        ext4_file_close(wf);
        return -1;
    } 
    return 0;
}

// 文件描述符复制
// 成功返回newfd 失败返回-1
uint64 ext4_sys_dup(int fd)
{
    proc_t* p = myproc();
    ext4_file_t* file;
    int newfd;

    file = p->ext4_ofile[fd];
    if(file == NULL) return -1;
    newfd = fd_alloc(file);
    ext4_file_dup(file);

    return newfd;
}

// 文件描述符复制(指定newfd)
// 成功返回newfd 失败返回-1
uint64 ext4_sys_dup2(int oldfd, int newfd)
{
    proc_t* p = myproc();
    ext4_file_t* file;
    //for (int i = 0; i < NOFILE; i++) {
    //    printf("fd[%d]: %p\n", i, p->ext4_ofile[i]);
    //}
    if(oldfd == newfd)
        return newfd;

    if((file = p->ext4_ofile[oldfd]) == NULL)
        return -1;

    if(p->ext4_ofile[newfd])
        ext4_file_close(p->ext4_ofile[newfd]);
    //printf("debug");
    p->ext4_ofile[newfd] = ext4_file_dup(file);

    return newfd;    
}

// 创建链接
// 成功返回0 失败返回-1
uint64 ext4_sys_linkat(int oldfd, char* oldpath, int newfd, char* newpath, int flags)
{
    ext4_inode_t *old_ref, *new_ref;
    if(get_refer(oldfd, oldpath[0], &old_ref) < 0)
        return -1;
    if(get_refer(newfd, newpath[0], &new_ref) < 0)
        return -1;
    return ext4_dir_link(oldpath, old_ref, newpath, new_ref, flags);
}

// 删除链接
// 成功返回0 失败返回-1
uint64 ext4_sys_unlinkat(int parfd, char* path, int flags)
{
    ext4_inode_t* ref;
    if(get_refer(parfd, path[0], &ref) < 0)
        return -1;
    return ext4_dir_unlink(path, ref);
}

// 设置时间戳
// 成功返回0 失败返回-1
uint64 ext4_sys_utimensat(int fd, char* path, uint64 addr_ts, int flags)
{
    ext4_inode_t* ref;
    ext4_file_t* file;
    timespec_t ts[2];

    if(get_refer(fd, path[0], &ref) < 0)
        return -1;
    if(addr_ts != 0 && uvm_copyin(myproc()->pagetable, (uint64)ts, addr_ts, sizeof(ts)) < 0)
        return -1;
    if((file = ext4_file_open(path, ref)) == NULL)
        return -2; // 这里返回-2代表文件不存在的错误
    assert(file->ip != NULL, "ext4_sys_utimensat: 0");
    /*
        TODO: ip的时间戳管理
    */
    ext4_file_close(file);
    return 0;
}

// 文件改名或者改位置
// 成功返回0 失败返回-1
uint64 ext4_sys_renameat2(int oldfd, char* oldpath, int newfd, char* newpath, int flags)
{
    ext4_inode_t *ip, *pip_1, *pip_2, *refer_1, *refer_2;
    char name_1[EXT4_NAME_LEN], name_2[EXT4_NAME_LEN];
    if(get_refer(newfd, newpath[0], &refer_1) < 0)
        return -1;
    if(get_refer(oldfd, oldpath[0], &refer_2) < 0)
        return -1;
    
    pip_1 = ext4_dir_path_to_pinode(oldpath, name_1, refer_1);
    if(pip_1 == NULL) return -1;

    pip_2 = ext4_dir_path_to_pinode(newpath, name_2, refer_2);
    if(pip_2 == NULL) {
        ext4_inode_put(pip_1);
        return -1;
    }
    
    ext4_inode_lock(pip_1);
    if(pip_1 != pip_2)
        ext4_inode_lock(pip_2);

    ip = ext4_dir_pinode_to_inode(pip_1, name_1);
    assert(ip != NULL, "ext4_sys_renameat2: 0");

    assert(ext4_dir_delete(pip_1, name_1) == 0, "ext4_sys_renameat2: 1");
    ext4_dir_create(pip_2, name_2, ip->inum, TYPE_REGULAR);
    
    ext4_inode_put(ip);
    ext4_inode_unlockput(pip_1);
    if(pip_1 != pip_2)
        ext4_inode_unlockput(pip_2);

    return 0;
}

#define	__S_IFDIR	0040000	/* Directory.  */
#define	__S_IFCHR	0020000	/* Character device.  */
#define	__S_IFBLK	0060000	/* Block device.  */
#define	__S_IFREG	0100000	/* Regular file.  */
#define	__S_IFIFO	0010000	/* FIFO.  */
#define	__S_IFLNK	0120000	/* Symbolic link.  */
#define	__S_IFSOCK	0140000	/* Socket.  */
#define	__S_IREAD	0400	/* Read by owner.  */
#define	__S_IWRITE	0200	/* Write by owner.  */
#define	__S_IEXEC	0100	/* Execute by owner.  */

static uint32 get_st_mode(uint16 ip_mode)
{
    uint32 mode = 0;

    if(ip_mode & IMODE_A_EXEC)
        mode |= __S_IEXEC;
    
    if(ip_mode & IMODE_A_READ)
        mode |= __S_IREAD;    

    if(ip_mode & IMODE_A_WRIT)
        mode |= __S_IWRITE;
    

    switch (ip_mode & IMODE_MASK)
    {
        case IMODE_CHAR:
            mode |= __S_IFCHR;
            break;
        case IMODE_FILE:
            mode |= __S_IFREG;
            break;
        case IMODE_DIR:
            mode |= __S_IFDIR;
            break;
        case IMODE_FIFO:
            mode |= __S_IFIFO;
            break;
        default:
            break;
    }
    return mode;
}

// 获取文件状态
// 成功返回0 失败返回-1
uint64 ext4_sys_fstat(int fd, uint64 dst)
{
    proc_t* p = myproc();
    ext4_file_t* file = p->ext4_ofile[fd];
    if(file == NULL || file->ip == NULL) return -1;

    file_stat_t st;
    st.st_dev = file->ip->dev;
    st.st_ino = file->ip->inum;
    st.st_mode = get_st_mode(file->ip->mode);
    st.st_nlink = file->ip->nlink;
    st.st_uid = st.st_gid = 0;
    st.st_rdev = file->major;
    st.st_size = file->ip->size;
    st.st_blksize = BLOCK_SIZE;
    st.st_blocks = (file->ip->size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    st.st_atim.sec = 0;
    st.st_atim.nsec = 0;
    st.st_mtim.sec = 0;
    st.st_mtim.nsec = 0;
    st.st_ctim.sec = 0;
    st.st_ctim.nsec = 0;
    return uvm_copyout(p->pagetable, dst, (uint64)&st, sizeof(st));
}

// fstatat的flags
#define AT_SYMLINK_NOFOLLOW 0x100  /* Do not follow symbolic links.  */
#define AT_REMOVEDIR        0x200  /* Remove directory instead of unlinking file.  */
#define AT_SYMLINK_FOLLOW   0x400  /* Follow symbolic links.  */
#define AT_NO_AUTOMOUNT     0x800  /* Suppress terminal automount traversal.  */
#define AT_EMPTY_PATH       0x1000 /* Allow empty relative pathname.  */

// 获取文件状态
// 成功返回0 失败返回-1
uint64 ext4_sys_fstatat(int fd, char* path, uint64 addr_stat, int flags)
{
    ext4_inode_t *refer;
    // assert(flags == AT_SYMLINK_NOFOLLOW, "ext4_sys_fstatat: 0");
    if(get_refer(fd, path[0], &refer) < 0)
        return -1;

    ext4_file_t* file = ext4_file_open(path, refer);
    if(file == NULL) return -2;
    fd = fd_alloc(file);

    uint64 ret = ext4_sys_fstat(fd, addr_stat);

    ext4_file_close(file);
    myproc()->ext4_ofile[fd] = NULL;

    return ret;
}

// mode 测试
#define R_OK 4 /* Test for read permission.  */
#define W_OK 2 /* Test for write permission.  */
#define X_OK 1 /* Test for execute permission.  */
#define F_OK 0 /* Test for existence.  */

// 文件权限测试
// 成功返回0 失败返回-1
uint64 ext4_sys_faccessat(int fd, char* path, int mode, int flags)
{
    ext4_inode_t *refer, *cur;
    uint64 ret = 0;

    assert(flags == 0, "ext4_sys_faccessat: 0");
    if(get_refer(fd, path[0], &refer) < 0)
        return -1;
    cur = ext4_dir_path_to_inode(path, refer);

    if(cur == NULL) return -1;
    if((mode & R_OK) && (!(cur->mode & IMODE_A_READ)))
        ret = -1;
    if((mode & W_OK) && (!(cur->mode & IMODE_A_WRIT)))
        ret = -1;
    if((mode & X_OK) && (!(cur->mode & IMODE_A_EXEC)))
        ret = -1;

    ext4_inode_put(cur);
    return ret;
}

struct statfs {
	int64 f_type;
	int64 f_bsize;
	uint64 f_blocks;
	uint64 f_bfree;
	uint64 f_bavail;
	uint64 f_files;
	uint64 f_ffree;
	int64 f_fsid;
	int64 f_namelen;
	int64 f_frsize;
	int64 f_flags;
	int64 f_spare[4]; // 保留位
};

// 获取文件系统信息
uint64 ext4_sys_statfs(char* path, uint64 addr_stat)
{
    struct statfs stat;
    stat.f_type = 0xef53;     // EXT4 文件系统标识
    stat.f_bsize = BLOCK_SIZE;
    stat.f_blocks = ext4_sb.block_count;
    stat.f_bfree = ext4_sb.block_count;
    stat.f_bavail = ext4_sb.block_count;
    stat.f_files = ext4_sb.inode_count;
    stat.f_ffree = ext4_sb.inode_count;
    stat.f_fsid = 0;
    stat.f_namelen = EXT4_NAME_LEN;
    stat.f_frsize = 0;
    stat.f_flags = 0;
    return uvm_copyout(myproc()->pagetable, addr_stat, (uint64)&stat, sizeof(stat));
}

// cmd取值
#define CMD_DUPFD 0
#define CMD_GETFD 1
#define CMD_SETFD 2
#define CMD_GETFL 3
#define CMD_SETFL 4
#define CMD_DUPFD_CLOEXEC 1030

static void set_exec_flag(ext4_file_t *file, int fd, bool flag)
{
  if(flag) {
    if(fd < 64)
      file->flags_low |= (1 << fd);
    else
      file->flags_high |= (1 << (fd % 64));
  } else {
    if(fd < 64)
      file->flags_low &= ~(1 << fd);
    else
      file->flags_high &= ~(1 << (fd % 64));
  }
}

static bool get_exec_flag(ext4_file_t *file, int fd)
{
  if(fd < 64)
    return ((file->flags_low & (1 << fd)) == 0 ? false : true);
  else
    return ((file->flags_high & (1 << (fd % 64))) == 0 ? false : true);
}

uint64 ext4_sys_fcntl(int fd, int cmd, int arg)
{
    proc_t* p = myproc();
    uint64 ret = 0;
    ext4_file_t* file = p->ext4_ofile[fd];
    if(file == NULL) return -1;

    switch (cmd)
    {
        case CMD_DUPFD:
            fd  = fd_alloc(file);
            ext4_file_dup(file);
            set_exec_flag(file, fd, false);
            ret = fd;
            break;
        case CMD_DUPFD_CLOEXEC:
            fd  = fd_alloc(file);
            ext4_file_dup(file);
            bool flag = get_exec_flag(file, fd);
            set_exec_flag(file, fd, flag);
            ret = fd;
            break;
        case CMD_GETFD:
            if(get_exec_flag(file, fd))
                ret = 1;
            else
                ret = 0;
            break;
        case CMD_SETFD:
            set_exec_flag(file, fd, (arg & 1) == 1);
            ret = 0;
            break;
        case CMD_GETFL:
            ret = file->oflags;
            break;
        case CMD_SETFL:
            file->oflags = arg;
            ret = 0;
            break;
        default:
            panic("ext4_sys_fcntl: 2");
            break;
    }
    return ret;
}


// 用于ppoll
typedef struct pollfd {
	int fd;	       /* file descriptor */
	short events;  /* requested events */
	short revents; /* returned events */
} pollfd_t;

#define POLLIN  0x001
#define POLLOUT 0x004

#define POLLERR  0x008  /* Error condition.  */
#define POLLHUP  0x010  /* Hung up. 如管道或Socket中，对端关闭了连接 */
#define POLLNVAL 0x020 /* Invalid polling request. fd未打开 */


// 等候某个事件发生
// 目前只用于pipe
uint64 ext4_sys_ppoll(uint64 addr_fds, int nfds, uint64 addr_ts, uint64 addr_sigmask)
{
    timespec_t ts;
    pollfd_t pfd;
    ext4_file_t* file = NULL;
    proc_t* p = myproc();

    if(addr_ts != 0) {
        if(uvm_copyin(p->pagetable, (uint64)&ts, addr_ts, sizeof(timespec_t)) < 0)
            return -1;
        
        timespec_t start, end;
        start = timer_get_ts(0);

        spinlock_acquire(&ticks_lk);
        while(1) {
            end = timer_get_ts(0);
            if(end.sec - start.sec >= ts.sec) break;
            if(proc_iskilled(p)) {
                spinlock_release(&ticks_lk);
                return -1;
            }
            proc_sleep(&ticks, &ticks_lk);
        }
        spinlock_release(&ticks_lk);
    }

    uint64 addr = 0, ret = 0;
    while(1) {
        addr = addr_fds;
        for(int i = 0; i < nfds; i++) {
            if(uvm_copyin(p->pagetable, (uint64)&pfd, addr, sizeof(pollfd_t)) < 0)
                return -1;
            pfd.revents = 0;
            // 目前只处理POLLIN和POLLOUT(用于支持pipe)
            if(pfd.fd >= 0) {
                file = p->ext4_ofile[pfd.fd];
                assert(file != NULL && file->file_type == TYPE_FIFO, "ext4_sys_ppoll: 0");
                
                spinlock_acquire(&file->pipe->lk);
                if(pfd.events & POLLIN) {
                    if(file->pipe->writeable == false) {   // 管道已经关闭
                        pfd.revents |= POLLHUP;
                    } else if(file->pipe->nread != file->pipe->nwrite) { // 管道中存在数据
                        pfd.revents |= POLLIN;
                    }
                }
                if(pfd.events & POLLOUT) {
                    if(file->pipe->readable == false) { // 管道已经关闭
                        pfd.revents |= POLLHUP;
                    } else if(file->pipe->nwrite - file->pipe->nread < EXT4_PIPE_SIZE) { // 管道中存在数据
                        pfd.revents |= POLLOUT;
                    }
                }
                spinlock_release(&file->pipe->lk);
            }
            uvm_copyout(p->pagetable, addr, (uint64)&pfd, sizeof(pollfd_t));
            addr += sizeof(pollfd_t);
            if(pfd.revents != 0) ret++;
        }
        if(ret || addr_ts) break;
        spinlock_acquire(&ticks_lk);
        proc_sleep(&ticks, &ticks_lk);
        spinlock_release(&ticks_lk);
    }
    return ret;
}
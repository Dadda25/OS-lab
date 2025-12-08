#include "syscall/sysfile.h"
#include "fs/fat32_sys.h"
#include "fs/fat32_file.h"
#include "fs/fat32_dir.h"
#include "fs/fat32_inode.h"
#include "fs/fat32_pipe.h"
#include "fs/base_stat.h"

#include "proc/cpu.h"
#include "mem/vmem.h"
#include "lib/print.h"
#include "lib/str.h"

extern FS_OP_t FS_OP;

// flags的可能取值
#define O_RDONLY    0x000
#define O_WRONLY    0x001
#define O_RDWR      0x002
#define O_CREATE    0x40
#define O_DIRECTORY 0x0200000

static uint8 flags_to_attr(int flags)
{
    uint8 attr = 0;
    if(flags & O_RDONLY) attr |= ATTR_READONLY;
    if(flags & O_DIRECTORY) attr |= ATTR_DIRECTORY;
    return attr;
}
// 在当前进程的打开文件表中找到一个空闲的fd
// 填充传入的文件指针 proc->fat32_ofile[fd] = file
// 成功返回获得的fd 失败返回-1
static int fd_alloc(fat32_file_t* file)
{
    proc_t* p = myproc();
    for(int i = 0; i < NOFILE; i++) {
        if(p->fat32_ofile[i] == NULL) {
            p->fat32_ofile[i] = file;
            return i;
        }
    }
    return -1;
}

// 成功返回0 失败返回-1
static int get_refer(int fd, char first, fat32_inode_t** refer)
{
    fat32_file_t* file;
    *refer = NULL;
    if(first != '/' && fd != FD_CWD) { // fd有效
        file = myproc()->fat32_ofile[fd];
        if(file == NULL || file->ip == NULL) return -1;
        *refer = file->ip;
    }
    return 0;
}

void fat32_sys_init()
{
    FS_OP.fs_getcwd = fat32_sys_getcwd;
    FS_OP.fs_chdir = fat32_sys_chdir;
    FS_OP.fs_mkdirat = fat32_sys_mkdirat;
    FS_OP.fs_getdents64 = fat32_sys_getdents64;
    FS_OP.fs_openat = fat32_sys_openat;
    FS_OP.fs_close = fat32_sys_close;
    FS_OP.fs_read = fat32_sys_read;
    FS_OP.fs_write = fat32_sys_write;
    FS_OP.fs_dup = fat32_sys_dup;
    FS_OP.fs_dup2 = fat32_sys_dup2;
    FS_OP.fs_pipe2 = fat32_sys_pipe2;
    FS_OP.fs_linkat = fat32_sys_linkat;
    FS_OP.fs_unlinkat = fat32_sys_unlinkat;
    FS_OP.fs_fstat = fat32_sys_fstat;
}

// 获得当前工作目录的路径
// 失败返回0, 成功返回dst
uint64 fat32_sys_getcwd(uint64 dst, int size)
{
    char path[PATH_LEN];
    char *s = path + PATH_LEN - 1;
    proc_t* p = myproc();
    uint32 name_len = 0, totol_len = 1;
    fat32_inode_t* ip = p->fat32_cwd;

    *s = '\0';
    if(ip->parent == NULL) {
        *(--s) = '/';
        totol_len++;
    }
    // 在child inode全部释放之前, parent inode一定存在于itable
    while(ip->parent != NULL) {
        
        assert(ip->valid == 1, "fat32_sys_getcwd: 0");
        name_len = strlen(ip->name);
        totol_len += name_len + 1;

        if(totol_len > size) return 0;
        if(totol_len > PATH_LEN) return 0; 
        
        memmove(s - name_len, ip->name, name_len);
        s -= name_len;
        *(--s) = '/';
        
        ip = ip->parent;
    }
    if(uvm_copyout(p->pagetable, dst, (uint64)s, totol_len) < 0) return 0;

    return dst;
}

// 改变工作目录
// 成功返回0, 失败返回-1
uint64 fat32_sys_chdir(char* path)
{
    fat32_inode_t* ip = fat32_dir_searchPath(path, NULL);
    if(ip == NULL) return -1;

    proc_t* p = myproc();
    fat32_inode_lock(ip);
    if(ip->attribute & ATTR_DIRECTORY) {
        fat32_inode_unlock(ip);
        fat32_inode_put(p->fat32_cwd);
        p->fat32_cwd = ip;
        return 0;
    }
    fat32_inode_unlockput(ip);
    return -1;
}

static struct {
    uint64 d_ino;	          // 索引结点号
    int64  d_off;	          // 到下一个dirent的偏移
    uint16 d_reclen;          // 当前dirent的长度
    uint8  d_type;	          // 文件类型
    char   name[DIR_LEN +1];  // 文件名
} de;

// 成功返回读取的字节数, 失败返回-1
uint64 fat32_sys_getdents64(int fd, uint64 dst, int len)
{
    proc_t* p = myproc();
    fat32_file_t* file = p->fat32_ofile[fd];
    if(file == NULL) return -1;
    if(file->type != FD_INODE || file->ip == NULL) return -1;
    if(!(file->ip->attribute & ATTR_DIRECTORY)) return -1;
    
    fat32_inode_t child;
    uint64 read_len = 0;
    int count = 0, off = 0, ret = 0;
    while(1) {
        ret = fat32_dir_getNextInode(file->ip, &child, off, &count);
        if(ret == 0) {        // 空目录项

        } else if(ret == 1) { // 有效目录项
            // 填充d
            uint32 namelen = strlen(child.name) + 1;
            de.d_ino = (uint64)child.first_clus;
            de.d_off = 19 + namelen;
            de.d_reclen = de.d_off;
            de.d_type = child.attribute;
            memmove(de.name, child.name, namelen);
            // 再读就要超出buf了
            if(read_len + de.d_reclen > len) break;
            // 数据转移
            ret = uvm_copyout(p->pagetable, dst, (uint64)&de, de.d_reclen);
            if(ret == -1) break;
            // 迭代
            read_len += de.d_reclen;
            dst += de.d_reclen;
        } else if(ret == -1) { // 读完了
            break;
        }
        off += count << 5;
    }
    return read_len;
}

// 成功返回0 失败返回-1
uint64 fat32_sys_mkdirat(int fd, char* path, uint16 mode)
{
    fat32_inode_t *refer, *pip, *ip;
    if(get_refer(fd, path[0], &refer) < 0)
        return -1;

    char name[DIR_LEN + 1];
    pip = fat32_dir_searchParPath(path, name, refer);
    if(pip == NULL) return -1;

    fat32_inode_lock(pip);
    ip = fat32_dir_createInode(pip, name, (uint8)ATTR_DIRECTORY);
    fat32_inode_unlockput(pip);    

    fat32_file_t* file = fat32_file_alloc();
    if(file == NULL) {
        fat32_inode_put(ip);
        return -1;
    }

    int newfd = fd_alloc(file);
    if(newfd == -1) {
        fat32_file_close(file);
        fat32_inode_put(ip);
        return -1;
    }

    file->ip = ip;
    file->off = 0;
    file->ref = 1;
    file->readable = true;
    file->writable = true;
    file->type = FD_INODE;
    return 0;
}

uint64 fat32_sys_openat(int fd, char* path, int flags, uint16 mode)
{
    printf("fat32_sys_openat: fd=%d, path=%s, flags=%d, mode=%o\n", fd, path, flags, mode);
    fat32_inode_t *ip, *refer;
    if(get_refer(fd, path[0], &refer) < 0) {
        printf("fat32_sys_openat: get_refer failed, fd=%d, path=%s\n", fd, path);
        return -1;
    }
    
    if(flags & O_CREATE) { // 创建文件
        char name[DIR_LEN + 1];
        fat32_inode_t* pip = fat32_dir_searchParPath(path, name, refer);
        ip = fat32_dir_createInode(pip, name, flags_to_attr(flags));
        fat32_inode_lock(ip);
    } else {               // 打开文件
        ip = fat32_dir_searchPath(path, refer);
        if(ip == NULL) {
            return -1; // 有效性检查
        }
        fat32_inode_lock(ip);
        if((ip->attribute & ATTR_READONLY) && !(flags & O_RDONLY)) goto fail;   // 只读检查
        if(!(ip->attribute & ATTR_DIRECTORY) && (flags & O_DIRECTORY)) goto fail; // 目录检查
    }

    fat32_file_t* file = fat32_file_alloc();
    if(file == NULL) goto fail;

    int newfd = fd_alloc(file);
    if(newfd == -1) {
        if(file) fat32_file_close(file);
        goto fail;
    }
    file->ip = ip;
    file->off = 0;
    file->readable = !(flags & O_WRONLY);
    file->writable = !(flags & O_RDONLY);
    file->ref = 1;
    file->type = FD_INODE;
    file->major = 0;

    fat32_inode_unlock(ip);
    return newfd;

fail:
    fat32_inode_unlockput(ip);
    return -1;   
}

// 成功返回0 失败返回-1
uint64 fat32_sys_close(int fd)
{
    fat32_file_t* file;
    proc_t* p = myproc();
    
    file = p->fat32_ofile[fd];
    if(file == NULL) return -1;

    fat32_file_close(file);
    if(file->ref == 0)
        p->fat32_ofile[fd] = NULL;
    
    return 0;
}

// 成功返回读取字节数 失败返回-1
uint64 fat32_sys_read(int fd, uint64 dst, int len)
{
    fat32_file_t* file;
    proc_t* p = myproc();

    file = p->fat32_ofile[fd];
    if(file == NULL) return -1;

    return fat32_file_read(file, dst, len);
}

// 成功返回写入的字节数 失败返回-1
uint64 fat32_sys_write(int fd, uint64 src, int len)
{
    fat32_file_t* file;
    proc_t* p = myproc();

    file = p->fat32_ofile[fd];
    if(file == NULL) return -1;

    return fat32_file_write(file, src, len);
}

// 成功返回0 失败返回-1
uint64 fat32_sys_pipe2(uint64 dst, int flags)
{
    proc_t* p = myproc();
    fat32_file_t *rf, *wf;
    int fd[2], ret;

    ret = fat32_pipe_alloc(&rf, &wf);
    if(ret < 0) return -1;

    fd[0] = fd_alloc(rf);
    if(fd[0] < 0) goto fail; 
    fd[1] = fd_alloc(wf);
    if(fd[1] < 0) {
        p->fat32_ofile[fd[0]] = NULL;
        goto fail;
    }

    ret = uvm_copyout(p->pagetable, dst, (uint64)fd, sizeof(fd));
    if(ret < 0) {
        p->fat32_ofile[fd[0]] = NULL;
        p->fat32_ofile[fd[1]] = NULL;
        goto fail;        
    } 

    return 0;

fail:
    fat32_file_close(rf);
    fat32_file_close(wf);
    return -1;
}

// 成功返回新复制的fd 失败返回-1
uint64 fat32_sys_dup(int fd)
{
    proc_t* p = myproc();
    fat32_file_t* file;
    
    file = p->fat32_ofile[fd];
    if(file == NULL) return -1;

    int new_fd = fd_alloc(file);
    if(fd < 0) return -1;

    fat32_file_dup(file);
    return new_fd;
}

// 成功返回新的描述符 失败返回-1
uint64 fat32_sys_dup2(int oldfd, int newfd)
{
    proc_t* p = myproc();
    fat32_file_t* file;
    
    file = p->fat32_ofile[oldfd];
    if(file == NULL) return -1;
    if(p->fat32_ofile[newfd] != NULL) return -1;
    
    p->fat32_ofile[newfd] = fat32_file_dup(file);
    return newfd;    
}

// 成功返回0 失败返回-1
uint64 fat32_sys_linkat(int oldfd, char* oldpath, int newfd, char* newpath, int flags)
{
    fat32_inode_t *old_ref, *new_ref;
    if(get_refer(oldfd, oldpath[0], &old_ref) < 0)
        return -1;
    if(get_refer(newfd, newpath[0], &new_ref) < 0)
        return -1;
    return fat32_dir_link(oldpath, old_ref, newpath, new_ref);
}

uint64 fat32_sys_unlinkat(int parfd, char* path, int flags)
{
    fat32_inode_t* ref;
    if(get_refer(parfd, path[0], &ref) < 0)
        return -1;
    return fat32_dir_unlink(path, ref);
}

static struct {
    uint64 st_dev;         // 设备号
    uint64 st_ino;         // inode序号
    uint32 st_mode;        // 文件模式
    uint32 st_nlink;       // 硬链接数
    uint32 st_uid;         // user id
    uint32 st_gid;         // group id
    uint64 st_rdev;        // device id (special file)
    uint64 __pad;          // 不重要
    uint64 st_size;        // 文件的大小(byte)
    uint32 st_blksize;     // block size
    int __pad2;            // 不重要
    uint64 st_blocks;      // 文件占用几个block
    long st_atime_sec;     // 最后一次访问时间-秒
    long st_atime_nsec;    // 最后一次访问时间-微秒
    long st_mtime_sec;     // 最后一次修改时间-秒
    long st_mtime_nsec;    // 最后一次修改时间-微秒
    long st_ctime_sec;     // 最后一次状态变化时间-秒
    long st_ctime_nsec;    // 最后一次状态变化时间-微秒
    unsigned __unused[2];
} fstat;

uint64 fat32_sys_fstat(int fd, uint64 dst)
{
    proc_t* p = myproc();
    fat32_file_t* file = p->fat32_ofile[fd];
    if(file == NULL) return -1;

    fstat.st_dev = (uint32)file->ip->dev;
    fstat.st_ino = file->ip->first_clus;
    fstat.st_mode = 0;
    fstat.st_nlink = 1;
    fstat.st_rdev = 0;
    fstat.st_uid = 0;
    fstat.st_gid = 0;
    fstat.st_size = file->ip->file_size;
    fstat.st_blksize = 512;
    fstat.st_blocks = file->ip->clus_cnt;
    fstat.st_atime_sec = 0;
    fstat.st_atime_nsec = 0;
    fstat.st_ctime_sec = 0;
    fstat.st_ctime_nsec = 0;
    fstat.st_mtime_sec = 0;
    fstat.st_mtime_nsec = 0;

    return uvm_copyout(p->pagetable, dst, (uint64)&fstat, sizeof(fstat));
}
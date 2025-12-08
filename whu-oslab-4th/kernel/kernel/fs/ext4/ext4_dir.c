/* EXT4 目录层 */

#include "fs/ext4.h"
#include "fs/ext4_dir.h"
#include "fs/ext4_inode.h" 
#include "proc/cpu.h"
#include "lib/str.h"
#include "lib/print.h"

extern ext4_inode_t ext4_rooti;

// pip是一个目录, 在这个目录里寻找名为filename的文件
// 找到了返回inode 没找到返回NULL
// 这个函数是从磁盘里读入inode的唯一路径
// 注意: pip应当有效且上锁
ext4_inode_t* ext4_dir_pinode_to_inode(ext4_inode_t* pip, char* filename)
{
    assert((pip->ref >= 1) && (pip->inum != 0), "ext4_dir_pinode_to_inode: 0");
    assert(sleeplock_holding(&pip->lk), "ext4_dir_pinode_to_inode: 1");
    assert((pip->mode & IMODE_MASK) == IMODE_DIR, "ext4_dir_pinode_to_inode: 2");
    assert(pip->size == BLOCK_SIZE, "ext4_dir_pinode_to_inode: 3");

    // "." 和 ".." 的处理
    if(strncmp(filename, ".", 1) == 0) {
        return ext4_inode_dup(pip);
    } else if(strncmp(filename, "..", 2) == 0) {
        if(pip == &ext4_rooti)
            return ext4_inode_dup(pip);
        else
            return ext4_inode_dup(pip->par);
    }

    ext4_inode_t* ip;
    ext4_dirent_t de;
    uint32 read_len, cut_len;

    // 先尝试在itable里面找
    ip = ext4_inode_search(pip, filename);
    if(ip) return ext4_inode_dup(ip);
    
    // 找不到再去磁盘里面找
    for(uint32 off = 0; off < BLOCK_SIZE - 12; off += de.len) { // "12"是为了空出最后一个"目录项"
        // 读取一个entry
        cut_len = min(sizeof(de), BLOCK_SIZE - 12 - off);
        read_len = ext4_inode_read(pip, off, cut_len, &de, false);
        assert(read_len == cut_len, "ext4_dir_pinode_to_inode: 4");
        if(de.inum == 0) continue; // 无效目录项

        // 找到目标
        de.name[de.name_len] = '\0';
        if(strncmp(filename, de.name, EXT4_NAME_LEN) == 0) {
            ip = ext4_inode_get();
            sleeplock_acquire(&ip->lk);

            ip->inum = de.inum;
            ip->par = pip;
            memmove(ip->name, de.name, de.name_len + 1);

            // path传递
            uint32 tmp = strlen(pip->path);
            uint32 tmp2 = strlen(ip->name);
            assert(tmp + tmp2 + 2 < PATH_LEN, "ext4_dir_pinode_to_inode: 5");
            strncpy(ip->path ,pip->path, tmp);
            strncpy(ip->path + tmp, ip->name, tmp2);
            ip->path[tmp + tmp2] = '/';
            ip->path[tmp + tmp2 + 1] = '\0';

            ext4_inode_readback(ip);
            sleeplock_release(&ip->lk);
            break;
        }
    } 
    return ip;
}

// 从path里面提取一个name，同时path向后更新
static char* skipelem(char *path, char *name)
{
    while (*path == '/') path++;

    if (*path == '\0') return NULL;
    
    char *s = path;
    while (*path != '/' && *path != '\0') path++;
    
    int len = path - s;
    if(len > EXT4_NAME_LEN) 
        len = EXT4_NAME_LEN;
    name[len] = 0;
    memmove(name, s, len);
    
    while (*path == '/') path++;

    return path;
}

// 寻找path对应的inode (或者parent inode)
static ext4_inode_t* lookup_path(char *path, char *name, ext4_inode_t* refer, bool parent)
{
    ext4_inode_t *cur, *next;

    if(*path == '/') {
        cur = ext4_inode_dup(&ext4_rooti); // 根目录
    } else if(*path != '\0') {
        if(refer == NULL)
            cur = ext4_inode_dup(myproc()->ext4_cwd); // 当前工作目录
        else
            cur = ext4_inode_dup(refer); // 指定目录
    } else {
        return NULL;
    }

    while ((path = skipelem(path, name)) != NULL) {

        ext4_inode_lock(cur);

        // cur不是目录
        if((cur->mode & IMODE_MASK) != IMODE_DIR) goto fail;

        // 找到目标父节点(此时entry是父节点, name是子节点文件名)
        if(parent && *path == '\0') {
            ext4_inode_unlock(cur);
            return cur;
        }

        // 尝试往下走一层
        next = ext4_dir_pinode_to_inode(cur, name);

        // 没有找到下一层inode
        if(next == NULL) goto fail;

        // 找到下一层inode
        ext4_inode_unlockput(cur);
        cur = next; 
    }

    if(parent) {
        ext4_inode_put(cur);
        return NULL;
    }
    
    return cur;

fail:
    ext4_inode_unlockput(cur);
    return NULL;
}

// 成功返回inode 失败返回NULL
ext4_inode_t* ext4_dir_path_to_inode(char* path, ext4_inode_t* refer)
{
    char name[EXT4_NAME_LEN];
    return lookup_path(path, name, refer, false);
}

// 成功返回pinode 失败返回NULL
ext4_inode_t* ext4_dir_path_to_pinode(char* path, char* name, ext4_inode_t* refer)
{
    return lookup_path(path, name, refer, true);
}

// 根据de.name_len计算de.len
// 四字节对齐 
uint16 ext4_dir_len(uint8 namelen)
{
    if(namelen % 4 != 0) // 对齐
        namelen += (4 - namelen % 4); 
    return (uint16)(namelen + 8); // 其他部分
}

// 返回以offset作为起始位置的dir_entry
// 注意: pip由调用者上锁
void ext4_dir_next(ext4_inode_t* pip, uint32 offset, ext4_dirent_t* de)
{
    assert((pip->ref >= 1) && (pip->inum != 0), "ext4_dir_next: 0");
    assert(offset < pip->size, "ext4_dir_next: 1");
    uint32 cut_len = min(sizeof(ext4_dirent_t), pip->size - offset);
    uint32 read_len = ext4_inode_read(pip, offset, cut_len, de, false);
    assert(read_len == cut_len, "ext4_dir_next: 2");
}

// ip此时是一个空的文件
// 这里要把它初始化成一个可用的目录
// 注意: 调用者负责上锁
void ext4_dir_init(ext4_inode_t* ip)
{
    assert(ip->mode & IMODE_DIR, "ext4_dir_init: 0");

    ext4_dirent_t tmp;
    tmp.file_type = TYPE_UNKNOWN;
    tmp.inum = 0;
    tmp.name[0] = '\0';
    tmp.name_len = 0;
    tmp.len = BLOCK_SIZE - 12;

    uint32 w_len = ext4_dir_len(tmp.name_len);
    sleeplock_acquire(&ip->lk);
    assert(ext4_inode_write(ip, 0, w_len, &tmp, false) ==  w_len, "ext4_dir_init: 1");
    ip->size = BLOCK_SIZE;
    sleeplock_release(&ip->lk);
}

// 在父目录pip下创建一个新的entry (修改data block)
// 注意: pip由调用者上锁
void ext4_dir_create(ext4_inode_t* pip, char* name, uint32 inum, uint8 file_type)
{
    assert(sleeplock_holding(&pip->lk), "ext4_dir_create: -3");
    assert((pip->mode & IMODE_MASK) == IMODE_DIR, "ext4_dir_create: -2");
    assert(pip->size == BLOCK_SIZE, "ext4_dir_create: -1");

    ext4_dirent_t de;
    uint8 slen = (uint8)strlen(name);
    uint16 len_1 = ext4_dir_len(slen), len_2 = 0, len = 0;
    uint32 off;

    for(off = 0; off < BLOCK_SIZE - 12; off += de.len) {
        ext4_dir_next(pip, off, &de);
        if(de.inum == 0 && de.len >= len_1) { // 找到一个够大的空闲空间 (之前的目录项被删后的空洞)
            break;
        } else if(off + de.len == BLOCK_SIZE - 12) {  // 最后一块区域
            len_2 = ext4_dir_len(de.name_len);
            len = de.len;
            if(len >= len_1 + len_2) {  // 足够大
                de.len = len_2;
                assert(ext4_inode_write(pip, off, len_2, &de, false) == de.len, "ext4_dir_create: 0");
                off += len_2;
                de.len = len - len_2;
                break;
            } else { // 不够大
                panic("ext4_dir_create: 1");
            }
        }
    }
    assert(off < BLOCK_SIZE - 12, "ext4_dir_create: 2"); // 非法情况

    // 目录项赋值并写入磁盘
    de.inum = inum;
    de.file_type = file_type;
    de.name_len = slen;
    memmove(de.name, name, (uint32)(slen));
    assert(ext4_inode_write(pip, off, len_1, &de, false) == len_1, "ext4_dir_create: 3");
}

// 在父目录pip下删除一个entry (修改data block)
// 成功返回0, 失败返回-1
// 注意: 调用者需持有pip的锁
int ext4_dir_delete(ext4_inode_t* pip, char* name)
{
    assert(sleeplock_holding(&pip->lk), "ext4_dir_delete: -1");
    
    ext4_dirent_t de;
    uint32 write_len;
    for(uint32 off = 0; off < BLOCK_SIZE - 12; off += de.len) {
        ext4_dir_next(pip, off, &de);
        // 无效目录项 
        if(de.inum == 0) continue;
        // 比较判定
        de.name[de.name_len] = '\0';
        if(strncmp(name, de.name, EXT4_NAME_LEN) == 0) {
            // 宣布这个entry无效并写回磁盘
            de.inum = 0;
            write_len = ext4_dir_len(de.name_len);
            assert(ext4_inode_write(pip, off, write_len, &de, false) == write_len, "ext4_dir_delete: 0");
            return 0;
        }
    } 
    return -1;
}

// 链接new_path到old_path对应的inode
// 成功返回0 失败返回-1
int ext4_dir_link(char* old_path, ext4_inode_t* old_refer, char* new_path, ext4_inode_t* new_refer, int flags)
{
    uint32 inum = 0;
    char name[EXT4_NAME_LEN];    
    ext4_inode_t* ip = ext4_dir_path_to_inode(old_path, old_refer);
    ext4_inode_t* pip = ext4_dir_path_to_pinode(new_path, name, new_refer);
    if(ip == NULL || pip == NULL) return -1;

    // ip->nlink++ 获取ip->inum
    ext4_inode_lock(ip);
    ip->nlink++;
    inum = ip->inum;
    ext4_inode_unlockput(ip);
    
    // pip 创建一个新的目录项
    ext4_inode_lock(pip);
    ext4_dir_create(pip, name, inum, mode_to_type(ip->mode));
    ext4_inode_unlockput(pip);

    return 0;    
}

// 解除这个dir_entry的链接
// 如果减到0则删除对应的inode
int ext4_dir_unlink(char* path, ext4_inode_t* refer)
{
    char name[EXT4_NAME_LEN];
    ext4_inode_t* pip = ext4_dir_path_to_pinode(path, name, refer);
    if(pip == NULL) return -1;

    ext4_inode_lock(pip);
    ext4_inode_t* ip = ext4_dir_pinode_to_inode(pip, name);
    if(ip == NULL) {
        ext4_inode_unlockput(pip);
        return -1;
    }

    ext4_inode_lock(ip);
    ip->nlink--;
    // 清除磁盘里的inode
    assert(ext4_dir_delete(pip, name) == 0, "ext4_dir_unlink: 0");
    
    ext4_inode_unlockput(ip);
    ext4_inode_unlockput(pip);

    return 0;
}
#include "fs/fat32_inode.h"
#include "fs/fat32_cluster.h"
#include "fs/fat32_dir.h"
#include "fs/base_stat.h"
#include "fs/fat32.h"
#include "proc/cpu.h"
#include "lib/str.h"
#include "lib/print.h"

extern fat32_inode_t fat32_rooti;
extern fat32_sb_t sb;

// 根据第一个clus遍历整条clus链
// 返回clus_cnt
static uint32 get_clusCnt(uint32 dev ,uint32 first_clus)
{
    uint32 next_clus = first_clus;
    uint32 count = 1;
    while(1) {
        next_clus = fat32_cluster_getNextCluster(dev, next_clus);
        if(next_clus >= 0x0FFFFFF8) break;
        count++;
    }
    return count;
}

// 读取目录项ld中的目录名到buf
// 目录项可以是长目录项,也可以是短目录项
static void read_entry_name(FAT32_longdir_t* ld, char* buf)
{
    if(ld->attribute == ATTR_LFN) { // 要读的是长目录名
        for(int i=0; i<5; i++)
            *buf++ = (char)(ld->name1[i]);
        for(int i=0; i<6; i++)
            *buf++ = (char)(ld->name2[i]);
        for(int i=0; i<2; i++)
            *buf++ = (char)(ld->name3[i]);
    } else { // 要读的是短目录名
        FAT32_shortdir_t* sd = (FAT32_shortdir_t*)ld;
        if(strncmp((char*)sd->name, ".", 1) != 0 &&
           strncmp((char*)sd->name, "..", 2) != 0)
        {
            panic("read_entry_name");
        }
        strncpy(buf, (char*)sd->name, 11);
    }
}

// 读取目录项de中的其他信息到inode
// 包括attribute first_clus file_size
// 同时设置cur_clus = first_clus && clus_cnt = 0
static void read_entry_info(fat32_inode_t* inode, FAT32_shortdir_t* de)
{
    inode->attribute = de->attribute;
    inode->first_clus = ((uint32)de->fisrt_cluster_high << 16) | (uint32)de->fisrt_cluster_low;
    inode->file_size = de->file_size;
    inode->cur_clus = inode->first_clus;
    inode->clus_cnt = get_clusCnt(inode->dev, inode->first_clus);
}

// 计算校验和(用于填写长目录名中的checksum字段)
static uint8 checksum(char* shortname)
{
	uint8 sum = 0;
    for (int i = 11; i != 0; i--)
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *shortname++;
    return sum;
}

// 检查name是否合法, 不合法则返回NULL
// 同时去掉前面的空格和点, 以及后面的空格
// 返回标准化后的name
static char* name_formate(char* name)
{
    static char illegal[] = { '\"', '*', '/', ':', '<', '>', '?', '\\', '|', 0};
    while (*name == ' ' || *name == '.') name++;
    
    int i, j;
    for(i=0; name[i] != '\0'; i++) {
        if(name[i] < 0x20) return NULL;
        for(j=0; j<9; j++) {
            if(name[i] == illegal[j]) 
                return NULL;
        }
    }
    
    while (name[--i]==' ');    
    name[i+1] = '\0';

    return name;    
}

// 输入name, 提取前面的部分作为shortname输出
static void name_getShortName(char* name, char* shortname)
{
	// 这些字符在longname中是合法的,但在shortname中非法
	static char illegal[] = {'+', ',', ';', '=', '[', ']', 0};
    int i = 0;
    char c, *p = name;
    for (int j = strlen(name) - 1; j >= 0; j--) {
        if (name[j] == '.') {
            p = name + j;
            break;
        }
    }
    while (i < 11 && (c = *name++)) {
        if (i == 8 && p) {
            if (p + 1 < name) { 
				break;
			} else {
                name = p + 1, p = 0;
                continue;
            }
        }

        if (c == ' ') continue;
        if (c == '.') {
            if (name > p) {
                memset(shortname + i, ' ', 8 - i);
                i = 8, p = NULL;
            }
            continue;
        }
        if (c >= 'a' && c <= 'z') { // 遇到小写字母转大写
            c += 'A' - 'a';
        } else if (strchr(illegal, c) != NULL) { // 遇到非法字符用下划线替代
            c = '_';
        }
        shortname[i++] = c;
    }
    while (i < 11) shortname[i++] = ' ';
}

static char *skipelem(char *path, char *name)
{
    while (*path == '/') {
        path++;
    }
    if (*path == 0) { return NULL; }
    
    char *s = path;
    

    while (*path != '/' && *path != 0) {
        path++;
    }
    
    int len = path - s;
    if (len > DIR_LEN) {
        len = DIR_LEN;
    }
    name[len] = 0;
    memmove(name, s, len);
    while (*path == '/') {
        path++;
    }
    return path;
}

static fat32_inode_t* lookup_path(char *path, bool parent, char *name, fat32_inode_t* refer)
{
    fat32_inode_t *entry, *next;
    
    if (*path == '/') {                         // 绝对路径
        entry = fat32_inode_dup(&fat32_rooti); 
    } else if (*path != '\0') {                 // 相对路径
        if(refer) entry = refer;
        else entry = fat32_inode_dup(myproc()->fat32_cwd);
    } else {
        return NULL;
    }

    while ((path = skipelem(path, name)) != 0) {
        fat32_inode_lock(entry);
        if (!(entry->attribute & ATTR_DIRECTORY)) {
            fat32_inode_unlockput(entry);
            return NULL;
        }
        if (parent && *path == '\0') {
            fat32_inode_unlock(entry);
            return entry;
        }
        next = fat32_dir_searchInode(entry, name, 0);
        if (next == NULL) {
            fat32_inode_unlock(entry);
            fat32_inode_put(entry);
            return NULL;
        }
        fat32_inode_unlock(entry);
        fat32_inode_put(entry);
        entry = next;
    }
    if (parent) {
        fat32_inode_put(entry);
        return NULL;
    }
    return entry;
}

/*------------------------------------------接口函数-----------------------------------------------------*/

// 根据路径查找inode
fat32_inode_t* fat32_dir_searchPath(char *path, fat32_inode_t* refer)
{
    char name[DIR_LEN + 1];
    return lookup_path(path, false, name, refer);
}

// 根据路径查找inode->parent
fat32_inode_t* fat32_dir_searchParPath(char *path, char *name, fat32_inode_t* refer)
{
    return lookup_path(path, true, name, refer);
}

// 从off偏移量开始查询parent管理的目录文件
// child中写入磁盘中inode信息, count中写入空闲entry数量
// 返回 0 -> next inode是count个空闲entry
// 返回 1 -> child = next inode count为child目录项总数
// 返回-1 -> 不合法的参数
int fat32_dir_getNextInode(fat32_inode_t* parent, fat32_inode_t* child, uint32 off, int* count)
{
    assert(parent->attribute & ATTR_DIRECTORY, "fat32_dir_getNextInode: 1\n");
    assert(parent->valid == 1, "fat32_dir_getNextInode: 2\n");
    assert(off % sizeof(FAT32_longdir_t) == 0, "fat32_dir_getNextInode: 3\n");
    assert(count != NULL, "fat32_dir_getNextInode: 4\n");

    memset(child->name, 0, DIR_LEN + 1);
    uint32 max_off = parent->clus_cnt * sb.byte_per_cluster;
    uint32 cnt = 0, ret = 0;
    FAT32_longdir_t ld;

    while(off < max_off) {

        // 读取parent目录下的一个entry
        ret = fat32_inode_relocate(parent, off, false);
        ret = fat32_cluster_read(parent->dev, parent->cur_clus, ret, sizeof(ld), (uint64)&ld, false);
        off += sizeof(ld);
        
        // case 1
        if(ld.order == 0x00) { // 检查这个entry是否是空闲的
            cnt++;
            continue;
        } else if(cnt != 0) {  // 连续cnt个空闲entry后遇到非空闲的entry
            *count = cnt;
            return 0;
        }

        // case 2
        if(ld.attribute == ATTR_LFN) {               // 这是一个长目录项
            int lcnt = ld.order & ~LAST_LONG_NAME;  // 拿到真正的order 
            if(ld.order & LAST_LONG_NAME) {         // 这是第一个长目录项
                *count = lcnt + 1;                  // 总的目录项 = 长目录项 + 短目录项
                count = NULL;                       // 为else服务
            }
            // 按序填写child->name
            read_entry_name(&ld, child->name + (lcnt - 1) * 13);
        } else {                                    // 这是一个短目录项 
            if(count) {                             // 如果前面没有读到长目录项
                *count = 1;                         // 总的目录项 = 短目录项
                read_entry_name(&ld, child->name);  // 填写child->name
            }
            // 这里总会执行到 因为一个文件的目录项
            // 要么是n个longdir + 1个shortdir
            // 要么是1个shortdir
            // 填写child中的name以外的信息
            read_entry_info(child, (FAT32_shortdir_t*)&ld);
            return 1;
        }
    }
    return -1;
}

// 在parent管理的目录文件中,查找名为filename的文件
// 如果找到了 则返回对应inode, poff不使用
// 如果没找到 则返回NULL, poff设为目录文件中第一个足够大的命名空间的偏移量
// 如果没找到且空间不够 则返回NULL, poff设为目录文件的最大偏移量
fat32_inode_t* fat32_dir_searchInode(fat32_inode_t* parent, char* filename, uint32 *poff)
{
    assert(parent->attribute & ATTR_DIRECTORY, "fat32_dir_searchInode: 1\n");

    if(parent->valid != 1) return NULL;

    // 特殊情况: filename = . 或者 ..
    if(strncmp(filename, ".", 1) == 0) {
        return fat32_inode_dup(parent);
    } else if (strncmp(filename, "..", 2) == 0) {
        if(parent == &fat32_rooti) {
            return fat32_inode_dup(parent);
        } else {
            return fat32_inode_dup(parent->parent);
        }
    }
    
    fat32_inode_t* child = fat32_inode_find_or_alloc(parent, filename);
    // 命中: itable中已经存在这样的child,直接返回
    if(child->valid == 1) return child; 
    
    // 未命中: 此时的child是一个空闲的节点(它的内容还在磁盘没有读入)
    int cnt = (strlen(filename) + 13 - 1) / 13; // 这个文件名占用几个longdir
    int count = 0, ret = 0; // count是inode_next的参数,ret是函数返回值
    uint32 off = ((parent == &fat32_rooti) ? 0 : 64);
    parent->cur_clus = parent->first_clus;

    while (ret != -1) { // count==-1说明off越界(整个目录区域都遍历完了)

        ret = fat32_dir_getNextInode(parent, child, off, &count);

        if(ret == 0) {                 // count个空闲entry可用
            if(poff && count > cnt) {  // case 2: 找到了一个足够大的空闲区域
                *poff = off;           // *poff就是这块空间的偏移量
                poff = NULL;
            }
        } else if(strncmp(filename, child->name, DIR_LEN) == 0) { // case 1: 被填充的child恰好是要找的
            child->offset = off;
            child->valid = 1;
            return child;
        }

        off += count << 5; // count个entry已经被查找过了
    }
    // case 3: 没有找到 同时 空闲空间不够存放
    if(poff) *poff = off;  // 赋予*poff一个不合理的值
    fat32_inode_put(child);      // 释放child
    return NULL;
}

// 在parent管理的目录文件的off偏移量处增加子节点child
// 如果offset=0或32则认为是添加 . 和 ..
// 否则添加若干长目录项和一个短目录项
void fat32_dir_addInode(fat32_inode_t* parent, fat32_inode_t* child, uint32 off)
{
    // 父节点应当是目录, 子节点的偏移量应当对齐, 父节点至少拥有一个cluster
    assert(parent->attribute & ATTR_DIRECTORY, "fat32_dir_addInode: 1\n");
    assert(off % sizeof(FAT32_shortdir_t) == 0, "fat32_dir_addInode: 2\n");
    assert(parent->first_clus != 0 && parent->clus_cnt != 0, "fat32_dir_addInode: 3\n");

    uint32 ret = 0;
    FAT32_shortdir_t sd;
    memset(&sd, 0, sizeof(sd));
    
    if(off == 0 || off == sizeof(sd)) { // 特殊目录项 . 和 ..
        // 短目录项生成
        if(off == 0) strncpy((char*)sd.name, ".", sizeof(sd.name));
        else         strncpy((char*)sd.name, "..", sizeof(sd.name));
        sd.attribute = ATTR_DIRECTORY;
        sd.file_size = 0;
        sd.fisrt_cluster_high = (uint16)(child->first_clus >> 16);
        sd.fisrt_cluster_low  = (uint16)(child->first_clus & 0xFFFF);
        // 短目录项写入
        ret = fat32_inode_relocate(parent, off, true);
        ret = fat32_cluster_write(parent->dev, parent->cur_clus, ret, sizeof(sd), (uint64)&sd, false);
        assert(ret == sizeof(sd), "fat32_dir_addInode: 4\n");
    
    } else { // 一般的目录项: 写入的内容包括若干长目录项和一个短目录项
        
        // 提取短目录名
        char shortname[12];
        memset(shortname, 0, sizeof(shortname));
        name_getShortName(child->name, shortname);
        
        // 长目录项的生成
        FAT32_longdir_t* ld = (FAT32_longdir_t*)(&sd);
        int cnt = (strlen(child->name) + 13 - 1) / 13; // 需要拆分成多少个长目录项
        ld->checksum = checksum(shortname);
        ld->attribute = ATTR_LFN;

        for(int i = cnt; i > 0; i--) {
            // 处理order
            ld->order = (uint8)i; // 注意: 第一个长目录项的order反映了长目录项的总数
            if(ld->order == cnt)  // 需要 LAST_LONG_NAME 帮助解码
                ld->order |= LAST_LONG_NAME;
            // 处理name1 name2 name3
            char* p = child->name + (i-1) * 13; // 待处理的字符串片段
            char* w = NULL;                     // 工作指针
            bool end = false;
            for(int j = 0; j < 13; j++) {
                switch (j) {
                    case 0:  w = (char*)ld->name1; break; // 0 1 2 3 4
                    case 5:  w = (char*)ld->name2; break; // 5 6 7 8 9 10
                    case 11: w = (char*)ld->name3; break; // 11 12
                }
                if(end) { // 空闲位置填充0xFFFF
                    *w++ = 0xFF;
                    *w++ = 0xFF;
                } else { // ASCII编码 -> unicode编码
                    if(*p == 0) end = true;
                    *w++ = *p++;
                    *w++ = 0x00;
                }
            }
            // 长目录项的写入
            ret = fat32_inode_relocate(parent, off, true);
            ret = fat32_cluster_write(parent->dev, parent->cur_clus, ret, sizeof(sd), (uint64)ld, false);
            assert(ret == sizeof(sd), "fat32_dir_addInode: 5\n");
            off += ret;
        }

        // 短目录项的生成
        memset(&sd, 0, sizeof(sd));
        for(int i=0; i<11; i++)
            sd.name[i] = shortname[i];
        sd.attribute = child->attribute;
        sd.file_size = child->file_size;
        sd.fisrt_cluster_high = (uint16)(child->first_clus >> 16);
        sd.fisrt_cluster_low  = (uint16)(child->first_clus & 0xFFFF);
        // 短目录项的写入
        ret = fat32_inode_relocate(parent, off, true);
        ret = fat32_cluster_write(parent->dev, parent->cur_clus, ret, sizeof(sd), (uint64)&sd, false);
        assert(ret == sizeof(sd), "fat32_dir_addInode: 6\n");
    }
}

// 尝试在parent目录下创建一个名为name的文件,属性是attr
// 如果这个名字的文件本来就存在则直接返回
// 如果不存在则创建一个新的后返回
fat32_inode_t* fat32_dir_createInode(fat32_inode_t* parent, char* name, uint8 attr)
{
    // parent应当是个有效的目录
    assert(parent->valid == 1, "fat32_dir_createInode: 1\n");
    assert(parent->attribute & ATTR_DIRECTORY, "fat32_dir_createInode: 2\n");
    name = name_formate(name);
    assert(name != NULL, "fat32_dir_createInode: 3\n");

    // 检查这个名字的目录是否已经存在,若存在则直接返回
    uint32 off;
    fat32_inode_t* child = fat32_dir_searchInode(parent, name, &off);
    if(child != NULL) return child;
    assert(off < parent->clus_cnt * sb.byte_per_cluster, "fat32_dir_createInode: 4\n");
    
    // 若不存在则申请一个空闲inode
    child = fat32_inode_find_or_alloc(parent, NULL);

    fat32_inode_lock(child);
    child->attribute = attr;
    child->file_size = 0;
    child->first_clus = 0;
    child->cur_clus = 0;
    child->clus_cnt = 0;
    child->offset = off;
    strncpy(child->name, name, DIR_LEN);
    child->name[DIR_LEN] = '\0';

    if(attr == ATTR_DIRECTORY) { // 如果是目录文件申请一个cluster并写入 . 和 ..
        child->first_clus = fat32_cluster_alloc(parent->dev);
        assert(child->first_clus != 0, "fat32_dir_createInode: 4\n");
        child->cur_clus = child->first_clus;
        child->clus_cnt++;
        fat32_dir_addInode(child, child, 0);                         // 生成 .
        fat32_dir_addInode(child, parent, sizeof(FAT32_shortdir_t)); // 生成 ..
    } else {
        child->attribute |= ATTR_ARCHIVE;
    }
    // 在parent管理的目录文件中增加child
    fat32_dir_addInode(parent, child, off);
    
    child->valid = 1;
    fat32_inode_unlock(child);

    return child;
}

int fat32_dir_link(char* old_path, fat32_inode_t* old_refer, char* new_path, fat32_inode_t* new_refer)
{
    // // 对于被链接文件，只需nlink++即可
    // inode_t* ip = fat32_dir_searchPath(old_path, old_refer);
    // if(ip == NULL) return -1;
    // inode_lock(ip);
    // if(ip->attribute & ATTR_DIRECTORY) { // 不允许链接目录文件
    //     inode_unlockput(ip);
    //     return -1;
    // }
    // ip->nlink++;
    // inode_unlockput(ip);

    // // 获得new_path的父节点和文件名
    // char new_name[DIR_LEN + 1];
    // inode_t* dp = fat32_dir_searchParPath(new_path, new_name, new_refer);
    // if(dp == NULL) {
    //     inode_lock(ip);
    //     ip->nlink--;
    //     inode_unlockput(ip);
    //     return -1;
    // }

    // // 在父节点下创建一个新的文件(链接文件)
    // inode_lock(dp);
    // inode_t* nip = fat32_dir_createInode(dp, new_name, 0);
    // inode_unlockput(dp);
    
    // // 链接文件和被链接文件描述同一个文件内容
    // inode_lock(ip);
    // inode_lock(nip);
    // nip->attribute  = ip->attribute;
    // nip->first_clus = ip->first_clus;
    // nip->clus_cnt   = ip->clus_cnt;
    // nip->cur_clus   = ip->cur_clus;
    // nip->file_size  = ip->file_size;
    // nip->ref        = ip->ref;
    // nip->nlink      = ip->nlink; 
    // inode_unlockput(nip);
    // inode_unlockput(ip);
    return 0;
}

int fat32_dir_unlink(char* path, fat32_inode_t* refer)
{    
    fat32_inode_t* ip = fat32_dir_searchPath(path, refer);
    if(ip == NULL) return -1;

    fat32_inode_lock(ip->parent);
    fat32_inode_remove(ip);
    fat32_inode_unlock(ip->parent);
    fat32_inode_put(ip);

    return 0;
}
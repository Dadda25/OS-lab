#ifndef __EXT4_DIR_H__
#define __EXT4_DIR_H__

#include "common.h"

/*
    目录设计:
    entry-1 entry-2 entry-3 ..... entry-n(最后一个entry的len是假的, 用于填满空间) entry-end(12 Byte)
*/

typedef struct ext4_inode ext4_inode_t;

// ext4 目录项 (8 + 255 + 1(为了对齐))
typedef struct ext4_dirent {
    uint32 inum;              // 这个目录项对应的 inode num (若为0则是无效目录)
    uint16 len;               // 这个目录项的长度
    uint8 name_len;           // 文件名的长度
    uint8 file_type;          // 文件类型(TYPE_XXX)
    char name[EXT4_NAME_LEN]; // 文件名
} ext4_dirent_t;

ext4_inode_t* ext4_dir_pinode_to_inode(ext4_inode_t* pip, char* name);
ext4_inode_t* ext4_dir_path_to_inode(char* path, ext4_inode_t* refer);
ext4_inode_t* ext4_dir_path_to_pinode(char* path, char* name, ext4_inode_t* refer);
uint16        ext4_dir_len(uint8 namelen);
void          ext4_dir_next(ext4_inode_t* pip, uint32 offset, ext4_dirent_t* de);
void          ext4_dir_init(ext4_inode_t* ip);
void          ext4_dir_create(ext4_inode_t* pip, char* name, uint32 inum, uint8 file_type);
int           ext4_dir_delete(ext4_inode_t* pip, char* name);
int           ext4_dir_link(char* old_path, ext4_inode_t* old_refer, char* new_path, ext4_inode_t* new_refer, int flags);
int           ext4_dir_unlink(char* path, ext4_inode_t* refer);

#endif
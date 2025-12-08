#ifndef __DIR_H__
#define __DIR_H__

typedef struct fat32_inode fat32_inode_t;

int 	       fat32_dir_getNextInode(fat32_inode_t* parent, fat32_inode_t* child, uint32 off, int* count);
fat32_inode_t* fat32_dir_searchInode(fat32_inode_t* parent, char* filename, uint32 *poff);
void           fat32_dir_addInode(fat32_inode_t* parent, fat32_inode_t* child, uint32 off);
fat32_inode_t* fat32_dir_createInode(fat32_inode_t* parent, char* name, uint8 attr);
fat32_inode_t* fat32_dir_searchPath(char *path, fat32_inode_t* refer);
fat32_inode_t* fat32_dir_searchParPath(char *path, char* name, fat32_inode_t* refer);
int            fat32_dir_link(char* old_path, fat32_inode_t* old_refer, char* new_path, fat32_inode_t* new_refer);
int            fat32_dir_unlink(char* path, fat32_inode_t* refer);

#endif
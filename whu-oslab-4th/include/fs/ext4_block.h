#ifndef __EXT4_BLOCK_H__
#define __EXT4_BLOCK_H__

#include "common.h"

uint32 ext4_block_read(uint32 dev, uint32 block_num, uint32 off, uint32 len, void* dst, bool user_dst);
uint32 ext4_block_write(uint32 dev, uint32 block_num, uint32 off, uint32 len, void* src, bool user_src);
uint32 ext4_block_alloc(uint32 dev);
void   ext4_block_free(uint32 dev, uint32 block_num);
void   ext4_block_zero(uint32 dev, uint32 block_num);

#endif
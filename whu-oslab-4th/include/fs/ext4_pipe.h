#ifndef __EXT4_PIPE_H__
#define __EXT4_PIPE_H__

#include "lock/lock.h"

#define EXT4_PIPE_SIZE 4000

typedef struct ext4_pipe {
    spinlock_t lk;
    uint32 nread;
    uint32 nwrite;
    bool   readable;
    bool   writeable;
    uint8 data[EXT4_PIPE_SIZE];
} ext4_pipe_t;

typedef struct ext4_file ext4_file_t;

int  ext4_pipe_alloc(ext4_file_t** read, ext4_file_t** write);
void ext4_pipe_close(ext4_pipe_t* pi, bool write_port);
int  ext4_pipe_read(ext4_pipe_t* pi, uint64 dst, uint32 n, bool user_dst);
int  ext4_pipe_write(ext4_pipe_t* pi, uint64 src, uint32 n, bool user_src);

#endif
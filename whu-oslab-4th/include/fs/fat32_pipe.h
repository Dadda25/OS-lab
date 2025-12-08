#ifndef __FAT32_PIPE_H__
#define __FAT32_PIPE_H__

#include "lock/lock.h"

#define FAT32_PIPE_SIZE 4000

typedef struct pipe {
    spinlock_t lk;
    uint32 nread;
    uint32 nwrite;
    bool   readable;
    bool   writeable;
    char data[FAT32_PIPE_SIZE];
} pipe_t;

typedef struct fat32_file fat32_file_t;

int  fat32_pipe_alloc(fat32_file_t** read, fat32_file_t** write);
void fat32_pipe_close(pipe_t* pi, bool writeable);
int  fat32_pipe_write(pipe_t* pi, uint64 va, int n);
int  fat32_pipe_read(pipe_t* pi, uint64 va, int n);

#endif
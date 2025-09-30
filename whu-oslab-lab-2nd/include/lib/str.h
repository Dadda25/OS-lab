#ifndef __STR_H__
#define __STR_H__

#include "common.h"

// 字符串长度
int strlen(const char *s);

// 字符串复制
char *strcpy(char *dst, const char *src);

// 字符串比较
int strcmp(const char *s1, const char *s2);

// 内存设置
void *memset(void *dst, int c, uint64 n);

// 内存复制
void *memcpy(void *dst, const void *src, uint64 n);

// 内存比较
int memcmp(const void *v1, const void *v2, uint64 n);

#endif
#include "lib/str.h"

void memset(void* begin, uint8 data, uint32 n)
{
    uint8* L = (uint8*)begin;
    for (uint32 i = 0; i < n; i++) {
        L[i] = data;
    }
}

void memmove(void *dst, const void* src, uint32 n)
{
    const char *s = src;
    char *d = dst;
    while(n--) {
        *d = *s;
        d++;
        s++;
    }
}

// 内存复制
void memcpy(void *dst, const void* src, uint32 n)
{
    memmove(dst, src, n);
}

// 字符串p的前n个字符与q做比较
// 按照ASCII码大小逐个比较
// 相同返回0 大于或小于返回正数或负数
int strncmp(const char *p, const char *q, uint32 n)
{
    while(n > 0 && *p && *p == *q)
      n--, p++, q++;
    if(n == 0)
      return 0;
    return (uint8)*p - (uint8)*q;
}

// 从src中复制前n个字符到dst
void strncpy(char *dst, const char *src, uint32 n)
{
    while(n != 0 && (*dst++ = *src++) != 0)
        n--;
    while(n !=0) {
        *dst++ = 0;
        n--;
    }
}

uint32 strlen(const char *s)
{
    uint32 n;

    for(n = 0; s[n]; n++)
      ;
    return n;
}

char* strchr(const char *s, char c)
{
    for(; *s; s++)
      if(*s == c)
        return (char*)s;
    return 0;
}

// 字符串比较
int strcmp(const char *p, const char *q)
{
    while(*p && *p == *q)
        p++, q++;
    return (uint8)*p - (uint8)*q;
}

// 字符串复制
char* strcpy(char *dst, const char *src)
{
    char *original_dst = dst;
    while((*dst++ = *src++) != 0)
        ;
    return original_dst;
}

// 字符串连接
char* strcat(char *dst, const char *src)
{
    char *original_dst = dst;
    // 找到dst的末尾
    while(*dst)
        dst++;
    // 复制src到dst末尾
    while((*dst++ = *src++) != 0)
        ;
    return original_dst;
}

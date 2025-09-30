#include "lib/str.h"

int strlen(const char *s)
{
    int n;
    for (n = 0; s[n]; n++);
    return n;
}

char *strcpy(char *dst, const char *src)
{
    char *odst = dst;
    while ((*dst++ = *src++) != 0);
    return odst;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void *memset(void *dst, int c, uint64 n)
{
    char *cdst = (char*)dst;
    int i;
    for (i = 0; i < n; i++) {
        cdst[i] = c;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, uint64 n)
{
    const char *s;
    char *d;

    if ((int64)dst % sizeof(void*) == 0 && 
        (int64)src % sizeof(void*) == 0 && 
        n % sizeof(void*) == 0) {
        // 按字长拷贝（优化）
        const uint64 *ls = src;
        uint64 *ld = dst;
        for (; n >= sizeof(uint64); n -= sizeof(uint64)) {
            *ld++ = *ls++;
        }
        s = (char*)ls;
        d = (char*)ld;
    } else {
        s = src;
        d = dst;
    }
    
    // 按字节拷贝
    for (; n > 0; n--) {
        *d++ = *s++;
    }
    
    return dst;
}

int memcmp(const void *v1, const void *v2, uint64 n)
{
    const uint8 *s1, *s2;
    
    s1 = v1;
    s2 = v2;
    while (n-- > 0) {
        if (*s1 != *s2) {
            return *s1 - *s2;
        }
        s1++, s2++;
    }
    
    return 0;
}
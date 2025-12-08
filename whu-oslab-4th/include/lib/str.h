#ifndef __STRING_H__
#define __STRING_H__

#include "common.h"

void   memset(void* begin, uint8 data, uint32 n);
void   memmove(void* dst, const void* src, uint32 n);
void   memcpy(void* dst, const void* src, uint32 n);

int    strncmp(const char *p, const char *q, uint32 n);
int    strcmp(const char *p, const char *q);
void   strncpy(char *dst, const char *src, uint32 n);
char*  strcpy(char *dst, const char *src);
char*  strcat(char *dst, const char *src);
uint32 strlen(const char* s);
char*  strchr(const char *s, char c);

#endif
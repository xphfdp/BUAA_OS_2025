#ifndef _STRING_H_
#define _STRING_H_

#include <types.h>

void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);
const char *strchr(const char *s, int c);
int strcmp(const char *p, const char *q);
const char *strrchr(const char *s, int c);
char *strncpy(char *dst, const char *src, size_t n);
char *strcat(char *s1, const char *s2);
int isalnum(int c);
char *strncat(char *dest, const char *src, size_t n);
int strncmp(const char *str1, const char *str2, size_t n);
char *strpbrk(const char *str1, const char *str2);
char *strreplace(char *str, char oldletter, char newletter);

#endif

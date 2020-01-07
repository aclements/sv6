// C11 7.24
#pragma once

#include "compiler.h"
#include <stddef.h>

#ifdef __cplusplus
#define XV6_STRING_NOEXCEPT noexcept
#else
#define XV6_STRING_NOEXCEPT
#endif

BEGIN_DECLS

void *memcpy(void *s1, const void *s2, size_t n) XV6_STRING_NOEXCEPT;
void *memmove(void *s1, const void *s2, size_t n) XV6_STRING_NOEXCEPT;
int memcmp(const void *s1, const void *s2, size_t n);
void *memset(void *s, int c, size_t n);
void *memchr(const void *s, int c, size_t n);

char *strchr(const char *s, int c);
size_t strlen(const char *s);
char* strcpy(char*, const char*);
char* strncpy(char *s, const char *t, size_t n) XV6_STRING_NOEXCEPT;
int strcmp(const char*, const char*);
int strncmp(const char *p, const char *q, size_t n);
char *strstr(const char *haystack, const char *needle);

long int strtol(const char *s, char **endptr, int base);

// Not C11
void *mempcpy(void *dest, const void *src, size_t n);
char *safestrcpy(char *s, const char *t, size_t n);

END_DECLS

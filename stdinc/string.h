// C11 7.24
#pragma once

#include "compiler.h"
#include "types.h"

BEGIN_DECLS

void *memcpy(void *s1, const void *s2, size_t n);
void *memmove(void *s1, const void *s2, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memset(void *s, int c, size_t n);
char *strchr(const char *s, int c);
size_t strlen(const char *s);

// Not C11
void *mempcpy(void *dest, const void *src, size_t n);

END_DECLS

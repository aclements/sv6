// C11 7.24
#pragma once

#include "types.h"

void *memcpy(void * restrict s1, const void* restrict s2, size_t n);
void *memmove(void *s1, const void *s2, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memset(void *s, int c, size_t n);
size_t strlen(const char *s);

// Not C11
void *mempcpy(void *dest, const void *src, size_t n);

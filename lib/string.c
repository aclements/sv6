#include "amd64.h"

#include <string.h>

void*
memset(void *dst, int c, size_t n)
{
  stosb(dst, c, n);
  return dst;
}

int
memcmp(const void* s1, const void* s2, size_t n)
{
  const uint8_t* p1 = s1;
  const uint8_t* p2 = s2;
  for (size_t i = 0; i < n; i++)
    if (p1[i] != p2[i])
      return p1[i] - p2[i];
  return 0;
}

void *
memmove(void *dst, const void *src, size_t n)
{
  const char *s;
  char *d;

  s = src;
  d = dst;
  if (s < d && s + n > d) {
    s += n;
    d += n;
    if ((intptr_t)s%4 == 0 && (intptr_t)d%4 == 0 && n%4 == 0)
      __asm volatile("std; rep movsl\n"
              :: "D" (d-4), "S" (s-4), "c" (n/4) : "cc", "memory");
    else
      __asm volatile("std; rep movsb\n"
              :: "D" (d-1), "S" (s-1), "c" (n) : "cc", "memory");
    // Some versions of GCC rely on DF being clear
    __asm volatile("cld" ::: "cc");
  } else {
    if ((intptr_t)s%4 == 0 && (intptr_t)d%4 == 0 && n%4 == 0)
      __asm volatile("cld; rep movsl\n"
              :: "D" (d), "S" (s), "c" (n/4) : "cc", "memory");
    else
      __asm volatile("cld; rep movsb\n"
              :: "D" (d), "S" (s), "c" (n) : "cc", "memory");
  }
  return dst;
}

// memcpy exists to placate GCC.  Use memmove.
void*
memcpy(void *dst, const void *src, size_t n)
{
  return memmove(dst, src, n);
}

void*
mempcpy(void *dst, const void *src, size_t n)
{
  return memmove(dst, (void *)src, n) + n;
}

int
strncmp(const char *p, const char *q, size_t n)
{
  while (n > 0 && *p && *p == *q)
    n--, p++, q++;
  if (n == 0)
    return 0;
  return (uint8_t)*p - (uint8_t)*q;
}

char*
strncpy(char *s, const char *t, size_t n)
{
  char *os = s;
  while (n > 0 && (*s++ = *t++) != 0)
    n--;
  if (n > 0)
    *s = 0;
  return os;
}

// Like strncpy but guaranteed to NUL-terminate.
char*
safestrcpy(char *s, const char *t, size_t n)
{
  char *os = s;
  if (n <= 0)
    return os;
  while (--n > 0 && (*s++ = *t++) != 0)
    ;
  *s = 0;
  return os;
}

char*
strcpy(char *s, const char *t)
{
  char *os = s;
  while ((*s++ = *t++) != 0)
    ;
  return os;
}

char*
strchr(const char *s, int c)
{
  for (; *s; s++)
    if (*s == c)
      return (char*)s;
  return 0;
}

size_t
strlen(const char *s)
{
  size_t n;

  for (n = 0; s[n]; n++)
    ;
  return n;
}

int
strcmp(const char *p, const char *q)
{
  while (*p && *p == *q)
    p++, q++;
  return (uint8_t)*p - (uint8_t)*q;
}

#include "amd64.h"

#include <ctype.h>
#include <string.h>
#include <strings.h>

void*
memset(void *dst, int c, size_t n)
{
  char *d = dst;
  for (size_t i = 0; i < n; i++)
    d[i] = c;
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
  const char *s = src;
  char *d = dst;
  if (s < d && s + n > d) {
      s += n, d += n;
      while (n -- > 0) {
          *-- d = *-- s;
      }
  } else {
      while (n -- > 0) {
          *d ++ = *s ++;
      }
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

void*
memchr(const void *s, int c, size_t n)
{
  for (size_t i = 0; i < n; ++i)
    if (((unsigned char*)s)[i] == c)
      return &((unsigned char*)s)[i];
  return NULL;
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

char*
strstr(const char *str, const char *needle) 
{
  if (!*needle) return (char *) str;
  char *p1 = (char*)str;
  while (*p1) {
    char *start = p1, *p2 = (char*)needle;
    while (*p1 && *p2 && *p1 == *p2) {
      p1++;
      p2++;
    }
    if (!*p2)
      return start;
    p1 = start + 1;
  }
  return NULL;
}

int
strcasecmp(const char *p, const char *q)
{
  while (*p && tolower((unsigned char)*p) == tolower((unsigned char)*q))
    p++, q++;
  return tolower((unsigned char)*p) - tolower((unsigned char)*q);
}

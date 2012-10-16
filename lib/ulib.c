#include "types.h"
#include "user.h"
#include "amd64.h"

#include <fcntl.h>
#include <unistd.h>

char*
strncpy(char *s, const char *t, size_t n)
{
  int tlen = strlen((char *)t);
  memmove(s, (char *)t, n > tlen ? tlen : n);
  if (n > tlen)
    s[tlen] = 0;
  return s;
}

char*
strcpy(char *s, const char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (u8)*p - (u8)*q;
}

int
strncmp(const char *p, const char *q, size_t n)
{
  while(n > 0 && *p && *p == *q)
    n--, p++, q++;
  if(n == 0)
    return 0;
  return (u8)*p - (u8)*q;
}

unsigned int
strlen(const char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}

void*
memset(void *dst, int c, unsigned int n)
{
  stosb(dst, c, n);
  return dst;
}

char*
strchr(const char *s, char c)
{
  for(; *s; s++)
    if(*s == c)
      return (char*)s;
  return 0;
}

char*
gets(char *buf, int max)
{
  int i, cc;
  char c;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
    buf[i++] = c;
    if(c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

int
stat(char *n, struct stat *st)
{
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if(fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

int
fstatat(int dirfd, const char *n, struct stat *st)
{
  int fd;
  int r;

  fd = openat(dirfd, n, O_RDONLY);
  if(fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

int
atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

void*
memcpy(void *dst, const void *src, size_t n)
{
  return memmove(dst, (void *)src, n);
}

void*
memmove(void *vdst, const void *vsrc, size_t n)
{
  const char *src;
  char *dst;

  dst = (char*) vdst;
  src = (const char*) vsrc;
  while(n-- > 0)
    *dst++ = *src++;
  return vdst;
}

int
memcmp(const void* s1, const void* s2, size_t n)
{
  const u8* p1 = s1;
  const u8* p2 = s2;
  for (size_t i = 0; i < n; i++)
    if (p1[i] != p2[i])
      return p1[i] - p2[i];
  return 0;
}

int
open(const char *path, int omode, ...)
{
  va_list ap;
  va_start(ap, omode);
  mode_t mode = va_arg(ap, int);
  va_end(ap);
  return openat(AT_FDCWD, path, omode, mode);
}

int
mkdir(const char *path, mode_t mode)
{
  return mkdirat(AT_FDCWD, path, mode);
}

unsigned
sleep(unsigned secs)
{
  nsleep((uint64_t)secs * 1000000000);
  return 0;
}

extern void __cxa_pure_virtual(void);
void __cxa_pure_virtual(void)
{ 
  die("__cxa_pure_virtual");
}

struct proghdr *_dl_phdr;
size_t _dl_phnum;

void
usetup(u64 elf_phdr, u64 elf_phnum)
{
  _dl_phdr = (struct proghdr*) elf_phdr;
  _dl_phnum = elf_phnum;
  forkt_setup(getpid());

  extern void initmalloc(void);
  initmalloc();
}

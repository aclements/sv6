#include "types.h"
#include "user.h"
#include "amd64.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
stat(const char *n, struct stat *st)
{
  int fd;
  int r;

  fd = open(n, O_RDONLY | O_ANYFD);
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

long
atol(const char *s)
{
  long n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
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
  fprintf(stderr, "__cxa_pure_virtual");
  exit(-1);
}

struct proghdr *_dl_phdr;
size_t _dl_phnum;

void
usetup(u64 elf_phdr, u64 elf_phnum)
{
  _dl_phdr = (struct proghdr*) elf_phdr;
  _dl_phnum = elf_phnum;
  forkt_setup(getpid());
}

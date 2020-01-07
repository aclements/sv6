#include "types.h"
#include "user.h"
#include "amd64.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

char **environ;

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

  fd = open(n, O_RDONLY | O_ANYFD | O_CLOEXEC);
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

pid_t
fork(void)
{
  return fork_flags(0);
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

int
lstat(const char *path, struct stat *buf)
{
  // xv6 has no symlinks
  return stat(path, buf);
}

int
fstat(int fd, struct stat *st)
{
  return fstatx(fd, st, 0);
}

unsigned
sleep(unsigned secs)
{
  nsleep((uint64_t)secs * 1000000000);
  return 0;
}

struct proghdr *_dl_phdr;
size_t _dl_phnum;

void
__crt_main(uint64_t argc, char **argv, uint64_t elf_phdr, uint64_t elf_phnum)
{
  extern void __cpprt_init(void);
  extern void __cpprt_fini(void);

  _dl_phdr = (struct proghdr*) elf_phdr;
  _dl_phnum = elf_phnum;
  forkt_setup(getpid());
  __cpprt_init();

  // Run global initializers.  (Note that gcc 4.7 eliminated the
  // .ctors section entirely, but gcc has supported .init_array for
  // some time.)  The third argument is envp, which we don't use.
  extern void (*__preinit_array_start[])(int, char **, char **);
  extern void (*__preinit_array_end[])(int, char **, char **);
  for (size_t i = 0; i < __preinit_array_end - __preinit_array_start; i++)
      (*__preinit_array_start[i])(argc, argv, 0);

  extern void (*__init_array_start[])(int, char **, char **);
  extern void (*__init_array_end[])(int, char **, char **);
  for (size_t i = 0; i < __init_array_end - __init_array_start; i++)
      (*__init_array_start[i])(argc, argv, 0);

  // Run main
  extern int main(int argc, char **argv);
  int res = main(argc, argv);

  // Run atexit functions
  __cpprt_fini();

  // Run global destructors.
  extern void (*__fini_array_start[])(void);
  extern void (*__fini_array_end[])(void);
  for (size_t i = __fini_array_end - __fini_array_start; i-- > 0; )
      (*__fini_array_start[i])();

  exit(res);
}

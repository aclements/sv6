#include "types.h"
#include "user.h"
#include "lib.h"
#include "amd64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE stdout_s{ .fd = 1 };
static FILE stderr_s{ .fd = 2 };
FILE *stdout = &stdout_s;
FILE *stderr = &stderr_s;

FILE*
fdopen(int fd, const char *mode)
{
  FILE *fp;

  if (mode[0] != 'r')
    return 0;
  fp = (FILE*)malloc(sizeof(*fp));
  if (fp == 0)
    return 0;
  if (fstat(fd, &fp->stat))
    return 0;
  fp->fd = fd;
  fp->off = 0;
  fp->poff = 0;
  fp->pfill = mode[1] == 'p';
  return fp;
}

int
fclose(FILE *fp)
{
  int r;

  r = close(fp->fd);
  free(fp);
  return r;
}

size_t
fread(void *ptr, size_t size, size_t nmemb, FILE *fp)
{
  ssize_t r;

  r = pread(fp->fd, ptr, size*nmemb, fp->off);
  if (r < 0) {
    fp->err = 1;
    return 0;
  } else if (r == 0) {
    fp->eof = 1;
    return 0;
  }
  fp->off += r;
  return r;
}

int
feof(FILE *fp)
{
  return fp->eof;
}

int
ferror(FILE *fp)
{
  return fp->err;
}

int
fflush(FILE* stream)
{
  // We don't implement buffering for writable files, so this does
  // nothing.
  return 0;
}

#pragma once

#include <stdarg.h>
#include <sys/stat.h>

BEGIN_DECLS

typedef struct fstream {
  int fd;
  off_t off;
  off_t poff;
  struct stat stat;
  int err:1;
  int eof:1;
  int pfill:1;
} FILE;

extern FILE* stdout;
extern FILE* stderr;

int    fflush(FILE *stream);
FILE  *fdopen(int fd, const char *mode);
int    fclose(FILE *fp);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *fp);
int    feof(FILE *fp);
int    ferror(FILE *fp);

void printf(const char*, ...)
  __attribute__((__format__(__printf__, 1, 2)));
void fprintf(FILE*, const char*, ...)
  __attribute__((__format__(__printf__, 2, 3)));
void vfprintf(FILE *, const char *fmt, va_list ap);
void snprintf(char *buf, unsigned int n, const char *fmt, ...)
  __attribute__((__format__(__printf__, 3, 4)));
void vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
void dprintf(int, const char*, ...)
  __attribute__((__format__(__printf__, 2, 3)));
void vdprintf(int fd, const char *fmt, va_list ap);

/*
 * Why does POSIX believe rename() should live in stdio.h?  Unclear.
 * http://pubs.opengroup.org/onlinepubs/009695399/functions/rename.html
 */
int    rename(const char *oldpath, const char *newpath);

END_DECLS

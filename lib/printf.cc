#include "types.h"
#include "user.h"
#include <stdarg.h>
#include "fmt.hh"
#include "lib.h"
#include <stdio.h>

struct outbuf {
  char b[128];
  int  n;
  int  fd;
};

static void
flushoutbuf(struct outbuf* b)
{
  int i = 0;
  int r;
  
  while (b->n != 0) {
    r = write(b->fd, &b->b[i], b->n);
    if (r == 0 || r < 0) {
      b->n = 0;
    } else {
      b->n -= r;
      i += r;
    }
  }
}

static void
writeoutbuf(int c, void *arg)
{
  struct outbuf* b = (struct outbuf*)arg;
  if (b->n == NELEM(b->b))
    flushoutbuf(b);
  b->b[b->n] = c;
  b->n++;
}

void
fprintf(FILE *f, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vfprintf(f, fmt, ap);
  va_end(ap);
}

void
vfprintf(FILE *f, const char *fmt, va_list ap)
{
  vdprintf(f->fd, fmt, ap);
}

void
dprintf(int fd, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vdprintf(fd, fmt, ap);
  va_end(ap);
}

void
vdprintf(int fd, const char *fmt, va_list ap)
{
  struct outbuf b;

  b.n = 0;
  b.fd = fd;
  vprintfmt(writeoutbuf, (void*) &b, fmt, ap);
  flushoutbuf(&b);
}

void
printf(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
}

// Print to a buffer.
struct bufstate {
  char *p;
  char *e;
};

static void
writebuf(int c, void *arg)
{
  struct bufstate *bs = (bufstate*) arg;
  if (bs->p < bs->e) {
    bs->p[0] = c;
    bs->p++;
  }
}

void
vsnprintf(char *buf, size_t n, const char *fmt, va_list ap)
{
  struct bufstate bs = { buf, buf+n-1 };
  vprintfmt(writebuf, (void*) &bs, fmt, ap);
  bs.p[0] = '\0';
}

void
snprintf(char *buf, u32 n, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(buf, n, fmt, ap);
  va_end(ap);
}

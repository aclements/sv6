// Miscellaneous utilities

#include "libutil.h"

#include <stdarg.h>
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

static void __attribute__((noreturn))
vdie(const char* errstr, va_list ap)
{
  vfprintf(stderr, errstr, ap);
  fprintf(stderr, "\n");
#if defined(XV6_USER)
  exit();
#else
  exit(1);
#endif
}

void __attribute__((noreturn))
die(const char* errstr, ...)
{
  va_list ap;

  va_start(ap, errstr);
  vdie(errstr, ap);
}

void
edie(const char* errstr, ...)
{
  va_list ap;

  va_start(ap, errstr);
#ifdef XV6_USER
  // There is no errno on xv6
  vdie(errstr, ap);
#else
  vfprintf(stderr, errstr, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  fprintf(stderr, ": %s\n", strerror(errno));
  exit(1);
#endif
}

#if !defined(XV6_USER)
int
setaffinity(int c)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(c, &cpuset);
  if (sched_setaffinity(0, sizeof(cpuset), &cpuset) < 0)
    edie("setaffinity, sched_setaffinity failed");
  return 0;
}
#endif

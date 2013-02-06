// Miscellaneous utilities

#include "libutil.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void __attribute__((noreturn))
die(const char* errstr, ...)
{
  va_list ap;

  va_start(ap, errstr);
  vfprintf(stderr, errstr, ap);
  va_end(ap);
  fprintf(stderr, "\n");
#if defined(XV6_USER)
  exit();
#else
  exit(1);
#endif
}

// Miscellaneous libu utilities

#include "types.h"
#include "user.h"
#include "libu.h"

#include <stdarg.h>
#include <stdio.h>

void __attribute__((noreturn))
die(const char* errstr, ...)
{
  va_list ap;

  va_start(ap, errstr);
  vfprintf(stderr, errstr, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  exit();
}

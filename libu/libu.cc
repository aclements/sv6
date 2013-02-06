// Miscellaneous libu utilities

#include "types.h"
#include "user.h"
#include "libu.h"
#include <stdarg.h>

void __attribute__((noreturn))
die(const char* errstr, ...)
{
  va_list ap;

  va_start(ap, errstr);
  vfdprintf(2, errstr, ap);
  va_end(ap);
  fprintf(2, "\n");
  exit();
}

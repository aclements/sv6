#include <stdio.h>

static FILE stdout_s;
FILE *stdout = &stdout_s;

int
fflush(FILE* stream)
{
  return 0;
}

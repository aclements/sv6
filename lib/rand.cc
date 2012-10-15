#include <stdlib.h>
#include "amd64.h"

void
srand(unsigned int seed)
{
}

int
rand(void)
{
  return rdtsc();
}

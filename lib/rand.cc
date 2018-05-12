#include <stdlib.h>
#include "riscv.h"

void
srand(unsigned int seed)
{
}

int
rand(void)
{
  return rdcycle();
}

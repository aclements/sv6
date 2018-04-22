#include "types.h"
#include "riscv.h"
#include "kernel.hh"

u64 cpuhz;

void
microdelay(u64 delay)
{
  u64 tscdelay = (cpuhz * delay) / 1000000;
  u64 s = rdcycle();
  while (rdcycle() - s < tscdelay)
    ;
}

void
inithz(void)
{
  // TODO
  cpuhz = 1500 * 1000 * 1000;
}

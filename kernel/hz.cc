#include "types.h"
#include "amd64.h"
#include "kernel.hh"

u64 cpuhz;

void
microdelay(u64 delay)
{
  u64 tscdelay = (cpuhz * delay) / 1000000;
  u64 s = rdtsc();
  while (rdtsc() - s < tscdelay)
    nop_pause();
}

void
inithz(void)
{
  // TODO
  cpuhz = 1500 * 1000 * 1000;
}

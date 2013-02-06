#include "rnd.hh"

#include <unistd.h>

// Random number generator for randomized tests
static __thread uint64_t rseed;

uint64_t
rnd(void)
{
  if (rseed == 0)
    rseed = getpid();
  rseed = rseed * 6364136223846793005 + 1442695040888963407;
  return rseed;
}

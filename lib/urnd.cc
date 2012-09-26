#include "types.h"
#include "user.h"

// Random number generator for randomized tests
static u64 rseed;

u64
rnd(void)
{
  if (rseed == 0)
    rseed = getpid();
  rseed = rseed * 6364136223846793005 + 1442695040888963407;
  return rseed;
}


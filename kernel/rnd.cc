#include "types.h"
#include "kernel.hh"
#include "percpu.hh"
#include "rnd.hh"

struct state {
  u64 seed;
};

percpu<state,percpu_safety::internal> rstate;

u64
rnd(void)
{
  if (rstate->seed == 0) {
    rstate->seed = rdtsc();
  }
  rstate->seed = rstate->seed *  6364136223846793005 + 1442695040888963407;
  return rstate->seed;
}

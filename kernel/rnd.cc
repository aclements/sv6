#include "types.h"
#include "kernel.hh"
#include "percpu.hh"
#include "rnd.hh"
#include "amd64.h"

struct state {
  u64 seed;
 __padout__;
};

DEFINE_PERCPU(state, rstate);

u64
rnd(void)
{
  scoped_critical crit(NO_SCHED);
  if (rstate->seed == 0) {
#if CODEX
    rstate->seed = 0xdeadbeef;
#else
    rstate->seed = rdtsc();
#endif
  }
  rstate->seed = rstate->seed *  6364136223846793005 + 1442695040888963407;
  return rstate->seed;
}

#pragma once
#include "amd64.h"

struct uspinlock {
  volatile u32 locked;       // Is the lock held?
};

static void inline __attribute__((always_inline))
acquire(struct uspinlock *lk)
{
  while(__atomic_exchange_n(&lk->locked, 1, __ATOMIC_ACQ_REL) != 0)
    ;
}

static void inline __attribute__((always_inline))
release(struct uspinlock *lk)
{
  __atomic_exchange_n(&lk->locked, 0, __ATOMIC_ACQ_REL);
}

static int inline
tryacquire(struct uspinlock *lk)
{
  return __atomic_exchange_n(&lk->locked, 1, __ATOMIC_ACQ_REL) == 0;
}

static void inline
initlock(struct uspinlock *lk)
{
  lk->locked = 0;
}

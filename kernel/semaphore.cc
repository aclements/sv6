#include "semaphore.h"

void
semaphore::acquire(uint64_t permits)
{
  scoped_acquire l(&lock);
  while (count < permits) {
    cv.sleep(&lock);
  }
  count -= permits;
}

bool
semaphore::try_acquire(uint64_t permits, uint64_t nsec)
{
  scoped_acquire l(&lock);
  if (nsec == 0 && count < permits)
    return false;

  uint64_t now = nsectime();
  uint64_t target = now + nsec;
  while (count < permits && ((now = nsectime()) < target)) {
    cv.sleep_to(&lock, target);
  }
  if (count < permits)
    return false;
  count -= permits;
  return true;
}

void
semaphore::release(uint64_t permits)
{
  scoped_acquire l(&lock);
  count += permits;
  cv.wake_all();
}

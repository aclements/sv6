#pragma once

#include "spinlock.h"
#include "condvar.h"

struct semaphore
{
#ifdef __cplusplus
private:
#endif
  spinlock lock;
  condvar cv;
  uint64_t count;

#ifdef __cplusplus
public:
  // Construct an uninitialized semaphore.  This should be
  // move-assigned from an initialized semaphore before being used.
  // This is constexpr, so it can be used for global semaphores
  // without incurring a static constructor.
  constexpr semaphore()
    : lock(), cv(), count() { }

  // Construct a semaphore.
  constexpr semaphore(const char *name, uint64_t permits)
    : lock(name), cv(name), count(permits) { }

  // Semaphores cannot be copied.
  semaphore(const semaphore &o) = delete;
  semaphore &operator=(const semaphore &o) = delete;

  // Semaphores can be moved.
  semaphore(semaphore &&o) = default;
  semaphore &operator=(semaphore &&o) = default;

  NEW_DELETE_OPS(semaphore);

  // Acquire @c permits permits, blocking until successful.
  void acquire(uint64_t permits = 1);

  // Try to acquire @c permits permits, blocking for at most @c nsec
  // nanoseconds.
  bool try_acquire(uint64_t permits, uint64_t nsec = 0);

  // Release @c permits permits.
  void release(uint64_t permits = 1);

  // Acquire one permit and return a ::lock_guard that holds the
  // permit.
  lock_guard<semaphore> guard()
    __attribute__((warn_unused_result))
  {
    return lock_guard<semaphore>(this);
  }
#endif
};

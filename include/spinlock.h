#pragma once
#include "types.h"
#include "lockstat.h"
#ifdef __cplusplus
#include "cpputil.hh"           // For NEW_DELETE_OPS
#endif

#if LOCKSTAT
extern struct klockstat klockstat_lazy;
#endif

// Mutual exclusion lock.
struct spinlock {
  u32 locked;       // Is the lock held?

#if SPINLOCK_DEBUG
  // For debugging:
  const char *name;  // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
  uptr pcs[10];      // The call stack (an array of program counters)
                     // that locked the lock.
#endif

#if LOCKSTAT
  struct klockstat *stat;
#endif

#ifdef __cplusplus
  // Construct an uninitialized spinlock.  This should be
  // move-assigned from an initialized spinlock before being used.
  // This is constexpr, so it can be used for global spinlocks without
  // incurring a static constructor.
  constexpr spinlock()
    : locked(0)
#if SPINLOCK_DEBUG
    , name(nullptr), cpu(nullptr), pcs{}
#endif
#if LOCKSTAT
    , stat(nullptr)
#endif
  { }

  // Create a spinlock.  This is constexpr, so it can be used for
  // global spinlocks without incurring a static constructor.
  constexpr spinlock(const char *name, bool lockstat = false)
    : locked(0)
#if SPINLOCK_DEBUG
    , name(name), cpu(nullptr), pcs{}
#endif
#if LOCKSTAT
    , stat(lockstat ? &klockstat_lazy : nullptr)
#endif
  { }

  // Spinlocks cannot be copied.
  spinlock(const spinlock &o) = delete;
  spinlock &operator=(const spinlock &o) = delete;

  // Spinlocks can be moved (though moving a locked spinlock is
  // obviously not recommended).
  spinlock(spinlock &&o);
  spinlock &operator=(spinlock &&o);

#if LOCKSTAT
  ~spinlock();
#endif

  NEW_DELETE_OPS(spinlock);
#endif // __cplusplus
};

void            acquire(struct spinlock*);
int             tryacquire(struct spinlock*);
int             holding(struct spinlock*);
void            release(struct spinlock*);

#if SPINLOCK_DEBUG
#define lockname(s) ((s)->name ?: "null")
#else
#define lockname(s) ("unknown")
#endif

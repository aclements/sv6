#pragma once
#include "types.h"
#include "lockstat.h"
#include <assert.h>
#ifdef __cplusplus
#include <atomic>
#include "cpputil.hh"           // For NEW_DELETE_OPS
#endif

#if LOCKSTAT
extern struct klockstat klockstat_lazy;
#endif

#ifdef __cplusplus
// ::lock_guard represents lock ownership of a lockable object.  These
// objects are not copyable, but they are movable, so lock ownership
// may be transferred.
template<class Lock>
class lock_guard
{
  Lock *l_;

public:
  // Acquire lock @c l.
  lock_guard(Lock *l) : l_(l)
  {
    l_->acquire();
  }

  // Default constructor.
  constexpr lock_guard() : l_(nullptr) { }

  // Release lock.
  ~lock_guard()
  {
    release();
  }

  // ::lock_guard cannot be copied.
  lock_guard(const lock_guard &) = delete;
  lock_guard& operator=(const lock_guard &) = delete;

  // Move constructor transfers ownership of @c o's lock to this
  // ::lock_guard.
  lock_guard(lock_guard &&o) : l_(o.l_)
  {
    o.l_ = nullptr;
  }

  // Move assignment transfers ownership of @c o's lock to this
  // ::lock_guard.
  lock_guard& operator=(lock_guard &&o)
  {
    release();
    l_ = o.l_;
    o.l_ = nullptr;
    return *this;
  }

  // Explicitly release the lock held by this ::lock_guard.
  void release()
  {
    if (l_) {
      l_->release();
      l_ = nullptr;
    }
  }

  // Return true if this ::lock_guard holds a lock.
  explicit operator bool () const noexcept
  {
    return !!l_;
  }
};
#endif

#define USE_CODEX_IMPL CODEX

// Mutual exclusion lock.
struct spinlock {

// Is the lock held?
#if defined(__cplusplus) && !USE_CODEX_IMPL
  std::atomic<u32> locked;
#else
  // codex does not use atomic<u32>, to avoid
  // recursive instrumentation
  u32 locked;
#endif

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
  // Conflicts with constexpr
  // ~spinlock();
#endif

  NEW_DELETE_OPS(spinlock);

  void acquire();
  bool try_acquire();
  void release();

  lock_guard<spinlock> guard()
  {
    return lock_guard<spinlock>(this);
  }

#if SPINLOCK_DEBUG
  bool holding();
#endif
#endif // __cplusplus
};

#if SPINLOCK_DEBUG
#define lockname(s) ((s)->name ?: "null")
#else
#define lockname(s) ("unknown")
#endif

#ifdef __cplusplus
// Deprecated aliases for spinlock methods

static inline void
acquire(struct spinlock* lk)
{
  lk->acquire();
}

static inline int
tryacquire(struct spinlock* lk)
{
  return lk->try_acquire();
}

static inline void
release(struct spinlock* lk)
{
  lk->release();
}

#if SPINLOCK_DEBUG
static inline int
holding(struct spinlock* lk)
{
  return lk->holding();
}
#endif

class nolock {
public:
  void acquire() {}
  void release() {}
  bool try_acquire() { return true; }
};

// XXX(Austin) scoped_acquire is the old name for the
// spinlock-specific RAII lock holder.
typedef lock_guard<spinlock> scoped_acquire;
#endif

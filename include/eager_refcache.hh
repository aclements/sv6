// Hybrid eager/scalable cached reference counting
//
// This is a variation of refcache that implements "eager-able"
// reference counts.  Initially, these reference counts are scalable,
// but also never get collected.  A special "eagerify" operation
// switches them into non-scalable eager mode, at which point they
// will be collected immediately when they reach zero.
//
// Much like refcache, scalable counts are kept in per-core delta
// caches.  However, since we never collect scalable counts, there is
// no need for review lists, periodic eviction, or epochs.

#pragma once

#include "spinlock.hh"
#include "spercpu.hh"

namespace eager_refcache {
  enum {
    CACHE_SLOTS = 4096
  };

  enum mode {
    // In scalable mode, an object's reference count is the sum of its
    // global count and any per-core deltas for the object.
    mode_scalable,

    // In transitioning mode, an object's reference count is still
    // distributed like in scalable mode, but all ways are being
    // flushed to the global count.  Any ways that newly point to an
    // object in transitioning mode must immediately be flushed to the
    // object's global count.
    mode_transitioning,

    // In eager mode, an object's global count is its true count.
    // It's still possible for new ways to temporarily hold a delta
    // for this object (if an inc/dec operation was started before we
    // entered transitioning), but they will always be flushed back to
    // the global count before the inc/dec returns.
    mode_eager,
  };

  class referenced
  {
    // This object's mode.  This always goes from scalable to
    // transitioning to eager.
    std::atomic<mode> mode_;

    // Global reference count.  How exactly this reflects the true
    // reference count of this object depend on mode_.  In scalable
    // mode, this may go negative as a result of evictions.
    std::atomic<int64_t> refcount_;

    void do_inc(int64_t delta);

  public:
    referenced(uint64_t refcount = 1)
      : mode_(mode_scalable),
        refcount_(refcount) { }

    referenced(const referenced &o) = delete;
    referenced(referenced &&o) = delete;
    referenced &operator=(const referenced &o) = delete;
    referenced &operator=(referenced &&o) = delete;

    void inc();
    void dec();

    // Switch this object into eager mode.  The caller must ensure
    // that the object's reference count cannot be zero or become zero
    // during this call.
    void eagerify();

  protected:
    virtual void onzero() = 0;
  };

  // The reference delta cache.  There is one instance of class cache
  // per core.
  class cache
  {
    friend class referenced;

    struct way
    {
      spinlock lock;
      // This can be read optimistically without the lock held.  It
      // should always be accessed with memory_order_relaxed.  For
      // unused ways, this is nullptr (and delta is ignored).
      std::atomic<referenced*> obj;
      // This should only be accessed with lock held.
      int32_t delta;

      constexpr way() : obj(), delta() { }
    };

    // The ways of the cache.  This must be accessed with interrupts
    // disabled.
    way ways_[CACHE_SLOTS];

    // Return the way in which a particular object's delta could be stored.
    way *hash_way(referenced *obj)
    {
      // Hash based on Java's HashMap re-hashing function.
      std::uint64_t wayno = (uintptr_t)obj;
      wayno ^= (wayno >> 32) ^ (wayno >> 20) ^ (wayno >> 12);
      wayno ^= (wayno >> 7) ^ (wayno >> 4);
      wayno %= CACHE_SLOTS;
      return &ways_[wayno];
    }

  public:
    cache() = default;
    cache(const cache &o) = delete;
    cache(cache &&o) = delete;
    cache &operator=(const cache &o) = delete;
    cache &operator=(cache &&o) = delete;
  };

  // Per-CPU reference delta cache.  Since cache ways should always be
  // locked when they are accessed, this doesn't need protection.
  DECLARE_PERCPU(cache, mycache, NO_CRITICAL);

  inline void
  referenced::inc()
  {
    do_inc(1);
  }

  inline void
  referenced::dec()
  {
    do_inc(-1);
  }

  inline void
  referenced::do_inc(int64_t delta)
  {
    // Optimistically handle eager mode without going through the cache
    if (mode_.load(std::memory_order_relaxed) == mode_eager) {
      auto ref = (refcount_ += delta);
      if (delta < 0 && ref == 0)
        onzero();
      return;
    }

    // Get this object's way
    auto way = mycache->hash_way(this);
    auto guard = way->lock.guard();

    // Evict an existing object
    auto way_obj = way->obj.load(std::memory_order_relaxed);
    if (way_obj != this) {
      if (way_obj && way->delta) {
        way_obj->refcount_ += way->delta;
      }
      way->obj.store(this, std::memory_order_relaxed);
      way->delta = 0;
    }

    // Modify delta
    way->delta += delta;

    // If the object is still in scalable mode, then eagerify hasn't
    // happened yet, so we can leave the delta in the cache.
    if (mode_ == mode_scalable)
      return;

    // The object is either transitioning or in eager mode.  Since
    // eagerify may already have passed way, flush it ourselves.
    auto ref = (refcount_ += way->delta);
    way->obj.store(nullptr, std::memory_order_relaxed);

    // If our mode is eager, then eagerify has finished and the global
    // count is stable.  If the global count is stable and we just
    // dropped it to zero, free the object.
    if (delta < 0 && ref == 0 && mode_ == mode_eager) {
      guard.release();
      onzero();
    }
  }
}

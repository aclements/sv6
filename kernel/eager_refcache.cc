#include "eager_refcache.hh"

namespace eager_refcache {
  DEFINE_PERCPU(cache, mycache, NO_CRITICAL);
}

void
eager_refcache::referenced::eagerify()
{
  // Put this object into transitioning mode.  This means inc/dec
  // should flush changes out immediately, but that the global count
  // still isn't consistent (and hence a zero global count does not
  // mean a zero true count).  Since the caller must hold a reference,
  // transitioning mode also means that the true count cannot be zero.
  mode_ = mode_transitioning;

  // Find all delta caches containing this object.  (This is intended
  // to let the CPU speculate and reorder these loads as much as
  // possible.)
  char has_obj[NCPU];
  for (unsigned cpu = 0; cpu < ncpu; ++cpu) {
    // XXX Is GCC smart enough to lift the hash function out?
    auto way = mycache[cpu].hash_way(this);

    // Check if this way holds this object.  This is racy: if there's
    // a concurrent inc/dec, it may set this way immediately after
    // this check, but if this happens, it will only be temporary
    // since the inc/dec will observe the mode and flush.  In other
    // ways, as soon as we set the mode, the set of ways that *stably*
    // contain this object will strictly decrease.
    has_obj[cpu] = (way->obj.load(std::memory_order_relaxed) == this);
  }

  // Evict this object from all caches
  int64_t sum = 0;
  for (unsigned cpu = 0; cpu < ncpu; ++cpu) {
    if (!has_obj[cpu])
      continue;

    // Lock the way
    auto way = mycache[cpu].hash_way(this);
    auto guard = way->lock.guard();
    // It might have been evicted in the mean time
    if (way->obj.load(std::memory_order_relaxed) != this)
      continue;

    // Flush the way
    sum += way->delta;
    way->obj.store(nullptr, std::memory_order_relaxed);
  }
  refcount_ += sum;

  // We've now flushed all non-transient deltas.  Put the object in
  // eager mode to tell future inc/dec operations that a zero global
  // count means a zero true count.
  mode_ = mode_eager;
}

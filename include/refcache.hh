// Scalable cached reference counts
//
// This implements space-efficient, scalable reference counting using
// per-core reference delta caches.  Increment and decrement
// operations are expected to be core-local, especially for workloads
// with good locality.  In contrast with most scalable reference
// counting mechanisms, this requires space proportional to the sum of
// the number of reference counted objects and the number of cores,
// rather than the product, and the per-core overhead can be adjusted
// to trade off space and scalability by controlling the reference
// cache eviction rate.  Finally, this mechanism guarantees that
// objects will be garbage collected within an adjustable time bound
// of when their reference count drops to zero.
//
// In this scheme, each reference counted object has a global
// reference count, much like in regular reference counting.  In
// addition, each core maintains a local, fixed-size cache of deltas
// to object's reference counts.  The true reference count of an
// object is the sum of its global count and any local deltas found in
// per-core caches.  The value of the true count is generally unknown,
// except when it has been zero for an entire garbage collection
// round, at which point we know the object can be freed.  Hence, much
// like regular reference counting, we depend on the fact that once an
// object's reference count is zero, it cannot go back up.
//
// To detect a zero true reference count and perform garbage
// collection, we use a token-passing cache flushing scheme.  Cores
// pass a token around in an arbitrary but fixed order.  When a core
// has the token, it flushes all of the reference count deltas in its
// cache, applying these updates to the global reference counts of
// each object.  As a result, the global count of an object tracks but
// lags behind the (conceptual) true count.  Because of the skew
// introduced by token-passing, updates to the global count are not
// applied in order; however, an update is guaranteed to be applied
// globally within one round of it occurring locally, as shown in the
// following figure:
//
//             t ->
//     core 0  *     -   * 
//          1    * +       *
//          2      *         *
//          3        *         *
//     global  1 1 1 1 1 0 1 1 1
//     true    1 1 2 1 1 1 1 1 1
//
// Because of this reordering, a zero global reference count does not
// imply a zero true reference count, but because of the bounded
// delay, if the global reference count is zero and *stays* zero for
// an entire round, then the true reference count is zero.
//
// To detect this, when a core applies an update globally, if this
// drops an object's global count to zero, the core places the object
// on a local "review" list (unless it's already on another core's
// review list).  When that core next gets the token, it scans its
// review list.  Any objects that had a global reference count of zero
// for the entire round can be freed.  Because of skew, an object may
// have a had a non-zero global count during the round, but have a
// zero reference count again when the reviewer gets the token.  We
// call this a "dirty" zero.  For example,
//
//             t ->      A         B
//     core 0  *     -   *         *
//          1    * +       * +       *
//          2      *         *         *
//          3        *        -*         *
//     global  1 1 1 1 1 0 1 1 0 0 0 1 0 0
//     true    1 1 2 1 1 1 1 2 1 1 1 1 1 1
//
// At time A, core 0 will place the object on its review list.
// Between A and B, the global count becomes temporarily non-zero, but
// it is zero again at time B, when core 0 scans its review list, even
// though the true count is never zero.  We distinguish real zeros
// from dirty zeros by simply setting a dirty flag on an object if we
// apply an update to it, the global count drops to zero, and we find
// that the object is already on a core's review list.  When a core
// finds a dirty zero on its review list, it simply clears the dirty
// flag and re-enqueues the object to be reviewed again in the next
// round.
//
// Finally, since the per-core cache is fixed size, we must also
// handle capacity evictions.  The same mechanism used for periodic
// evictions applies to capacity evictions, except that we must be
// careful to wait at least an entire round before reviewing the
// object.  In practice, this means we can't simply add the object to
// the review list, since this will cause it to get reviewed at the
// end of the *current* round, before the token has passed through all
// cores.  Instead, we add the object to another list that becomes the
// initial review list for the next round.

#pragma once

#include "spinlock.h"
#include "ilist.hh"
#include "percpu.hh"

#ifndef REFCACHE_DEBUG
#define REFCACHE_DEBUG 1
#endif

namespace refcache {
  enum {
    CACHE_SLOTS = 4096
  };

  // Base class for an object that's reference counted using the
  // refcaching scheme.
  class referenced
  {
  private:
    friend class cache;

    // XXX This can all be packed into two words

    // This lock protects all of the following fields.
    spinlock lock_;

    // Global reference count.  This object's true reference count is
    // the sum of this and each core's cached reference delta for this
    // object.  In general, we don't know what the true reference
    // count is except in one situation: when it's been zero for long
    // enough, we know it's zero (and will stay zero).
    //
    // Since the global count can go negative, this must be signed.
    int64_t refcount_;

    // Link used to track this object in the per-core review list or
    // next-review list.
    islink<referenced> next_;
    typedef islist<referenced, &referenced::next_> list;

    // True if this object is on some core's review list.
    bool has_reviewer_;

    // True if this object's refcount was non-zero and then zero again
    // since it was last reviewed.
    bool dirty_;

  public:
    constexpr referenced(uint64_t refcount = 1)
      : lock_("refcache::referenced"),
        refcount_(refcount),
        next_(),
        has_reviewer_(false),
        dirty_(false) { }

    referenced(const referenced &o) = delete;
    referenced(referenced &&o) = delete;
    referenced &operator=(const referenced &o) = delete;
    referenced &operator=(referenced &&o) = delete;

    void inc();
    void dec();

  protected:
    // We could eliminate these virtual methods and the vtable
    // altogether with a somewhat more complicated template-based
    // scheme in which referenced is a template over the type T being
    // referenced-counted.  referenced would have a static member
    // storing a "manager" object specialized for each type.  This
    // manager would statically call the appropriate methods of T.
    // This would require storing a pointer to the manager alongside
    // *generic* pointers to referenced objects (such as in the
    // cache's hash table), but wouldn't require the vtable pointer in
    // each object or the overhead of virtual method calls.

    virtual ~referenced() { };
    virtual void onzero() { delete this; }
  };

  // The reference delta cache.  There is one instance of class cache
  // per core.
  class cache
  {
    friend class referenced;

    struct way
    {
      referenced *obj;
      // XXX Deltas could be much smaller (say int8_t).  There are
      // several advantages to this, especially if we have higher
      // associativity, because we can pack more of them into a cache
      // line and find used or unused slots faster.  If the delta
      // overflows or underflows, we can simply evict it.
      int64_t delta;

      constexpr way() : obj(), delta() { }
    };

    // The ways of the cache.  This must be accessed with interrupts
    // disabled to prevent interference between a review process and
    // capacity evictions.
    way ways_[CACHE_SLOTS];

    // The list of objects to review the next time this core gets the
    // round token.  This must be accessed only by the review process.
    // Since the review process is pinned and naturally serialized, no
    // protection is necessary.
    referenced::list review_;

    // The head of the list of objects to review two rounds from
    // now.  Before passing the token on, this becomes the review
    // list.  This must be accessed with interrupts disabled.
    referenced::list next_;

    // Place obj in the cache if necessary and return its assigned
    // way.  Interrupts must be disabled.
    way *get_way(referenced *obj)
    {
      // XXX Hash pointer better?  This isn't bad: it's very fast and
      // shouldn't suffer from small alignments.
      std::size_t wayno = (((uintptr_t)obj) | ((uintptr_t)obj / CACHE_SLOTS))
        % CACHE_SLOTS;
      struct way *way = &ways_[wayno];
      // XXX More associativity.  Since this is in the critical path
      // of every reference operation, perhaps we should do something
      // like hash-rehash caching?
      if (way->obj != obj) {
        // This object is not in the cache
        if (way->delta) {
          // Need to evict to free up an entry.  Since this is a
          // capacity eviction, we need to review this object not the
          // next time we get the token, but one round *after* that to
          // ensure that the token has passed through all cores.
          evict(way, &next_);
        }
        // Take this entry
        way->obj = obj;
      }
      return way;
    }

    // Evict the object from way, freeing up this slot.  list must be
    // either &review_ for periodic evictions or &next_ for capacity
    // evictions.  way must have a non-zero delta and interrupts must
    // be disabled.
    void evict(struct way *way, referenced::list *list);

    // Scan this core's review list and perform periodic eviction.
    // The calling thread must be pinned (but interrupts may be
    // enabled).
    void review();

  public:
    constexpr cache() = default;
    cache(const cache &o) = delete;
    cache(cache &&o) = delete;
    cache &operator=(const cache &o) = delete;
    cache &operator=(cache &&o) = delete;

    // Refcache worker thread.  There should be a single instance of
    // this.  This thread is the token: it migrates through the cores
    // performing periodic eviction.
    static void worker(void *);
  };

  // Per-CPU reference delta cache.  In general this has to be
  // accessed with interrupts disabled or by a pinned process to
  // prevent migration.  Some fields of cache specifically require
  // interrupts to be disabled to prevent concurrent access.
  // XXX Allocation of this should be NUMA-aware
  extern percpu<cache> mycache;

  inline void
  referenced::inc()
  {
    pushcli();
    ++mycache->get_way(this)->delta;
    popcli();
  }

  inline void
  referenced::dec()
  {
    pushcli();
    --mycache->get_way(this)->delta;
    popcli();
  }
}

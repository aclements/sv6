// Scalable cached reference counts
//
// This implements space-efficient, scalable reference counting using
// per-core reference delta caches.  Increment and decrement
// operations are expected to be core-local, especially for workloads
// with good locality.  In contrast with most scalable reference
// counting mechanisms, refcache requires space proportional to the
// sum of the number of reference counted objects and the number of
// cores, rather than the product, and the per-core overhead can be
// adjusted to trade off space and scalability by controlling the
// reference cache eviction rate.  Finally, this mechanism guarantees
// that objects will be garbage collected within an adjustable time
// bound of when their reference count drops to zero.
//
// In refcache, each reference counted object has a global reference
// count (much like a regular reference count) and each core also
// maintains a local, fixed-size cache of deltas to object's reference
// counts.  Incrementing or decrementing an object's reference count
// modifies only the local, cached delta and this delta is
// periodically flushed to the object's global reference count.  The
// true reference count of an object is thus the sum of its global
// count and any local deltas for that object found in the per-core
// caches.  The value of the true count is generally unknown, but we
// assume that once it drops to zero, it will remain zero.  We depend
// on this stability to detect a zero true count after some delay.
//
// To detect a zero true reference count, refcache divides time into
// periodic *epochs* during which each core flushes all of the
// reference count deltas in its cache, applying these updates to the
// global reference count of each object.  The last core in an epoch
// to finish flushing its cache ends the epoch and after some delay
// (our implementation uses 10ms) all of the cores repeat this
// process.  Since these flushes occur in no particular order and the
// caches batch reference count changes, updates to the reference
// count can be reordered.  As a result, a zero global reference count
// does not imply a zero true reference count.  However, once the true
// count *is* zero, there will be no more updates, so if the global
// reference count of an object drops to zero and *remains* zero for
// an entire epoch, then the refcache can guarantee that the true
// count is zero and free the object.
//
// This lag between the true reference count and the global reference
// count of an object is the main complication for refcache.  For
// example, consider the following sequence of increments, decrements,
// and flushes for a single object:
//
//         t ->
// core 0     -   *   |       *     * +   |   * +     |
//      1   +         *     * |     *     |       - * |
//      2           * |     * |           *         * |
//      3       *     |     * |         - *           *
// global  1 1 1 1 0 0 1 1 1 1 1 1 1 1 1 1 0 0 1 1 1 0 0
// true    1 2 1 1 1 1 1 1 1 1 1 1 1 1 2 1 1 1 1 2 1 1 1
// epoch   ^----1-----^---2---^-----3-----^-----4-----^
//
// Because of flush order, the two updates in epoch 1 are applied to
// the global reference count in the opposite order of how they
// actually occurred.  As a result, core 0 observes the global count
// temporarily drop to zero when it flushes in epoch 1, even though
// the true count is non-zero.  This is remedied as soon as core 1
// flushes its increment delta, and when core 0 reexamines the object
// at the end of epoch 2, after all cores have again flushed their
// reference caches, it can see that the global count is non-zero and
// hence the zero count it observed was not a true zero and the object
// should not be freed.
//
// It is not enough for the global reference count to be zero when an
// object is reexamined; rather, it must have been zero for the entire
// epoch.  For example, core 0 will observe a zero global reference
// count at the end of epoch 3, and again when it reexamines the
// object at the end of epoch 4.  However, the true count is not zero,
// and the global reference count was temporarily non-zero during the
// epoch.  We call this a *dirty* zero and in this situation the
// refcache will queue the object to be examined again after another
// epoch.
//
// The pseudocode for refcache is given below.  Each core maintains a
// hash table storing its reference delta cache and a "review" queue
// that tracks objects whose global reference counts reached zero.  A
// core reviews an object once it can guarantee that all cores have
// flushed their reference caches after it put the object in its
// review queue.
//
//   flush():
//     evict all cache entries
//     update the current epoch
//
//   evict(object, delta):
//     object.refcnt <- object.refcnt + delta
//     if object.refcnt = 0:
//       if object is not on any review queue:
//         object.dirty <- false
//         add (object, epoch) to the local review queue
//       else:
//         object.dirty <- true
//
//   review():
//     for each (object, oepoch) in local review queue:
//       if oepoch <= epoch + 2:
//         remove object from the review queue
//         if object.refcnt = 0:
//           if object.dirty:
//             object.dirty <- false
//             add (object, epoch) to the review queue
//           else:
//             free object
//
// For epoch management, our current implementation uses a simple
// barrier scheme that tracks a global epoch counter, per-core epochs,
// and a count of how many per-core epochs have reached the current
// global epoch.  This scheme suffices for our benchmarks, but more
// scalable schemes are possible, such as the tree-based quiescent
// state detection scheme used by Linux's hierarchical RCU
// implementation [http://lwn.net/Articles/305782/].

#pragma once

#include "spinlock.h"
#include "ilist.hh"
#include "percpu.hh"
#include "kstats.hh"
#include "condvar.h"

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

    // XXX This can all be packed into three words

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

    // Link used to track this object in the per-core review list.
    islink<referenced> next_;
    typedef isqueue<referenced, &referenced::next_> list;

    // If this object is on a review list, the epoch in which this
    // object can be reviewed.  0 if this object is not on a review
    // list.
    uint64_t review_epoch_;

    // True if this object's refcount was non-zero and then zero again
    // since it was last reviewed.
    bool dirty_;

  public:
    constexpr referenced(uint64_t refcount = 1)
      : lock_("refcache::referenced"),
        refcount_(refcount),
        next_(),
        review_epoch_(0),
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

    // The list of objects to review in increasing epoch order.  This
    // must be accessed only by the local core and there must be at
    // most one reviewer at a time per core.
    referenced::list review_;

    // The list of objects whose onzero() method should be called.  Call
    // onzero() from a separate thread, instead of the timer interrupt,
    // to avoid deadlock with the thread preempted by the timer.
    referenced::list reap_;
    spinlock reap_lock_;
    condvar reap_cv_;

    // The last global epoch number observed by this core.
    uint64_t local_epoch;

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
          // capacity eviction, local_epoch may be behind
          // global_epoch.
          evict(way, false);
          kstats::inc(&kstats::refcache_conflict_count);
        }
        // Take this entry
        way->obj = obj;
      }
      return way;
    }

    // Evict the object from way, freeing up this slot.  way must have
    // a non-zero delta and interrupts must be disabled.  If
    // local_epoch_is_exact, then we assume that local_epoch equals
    // global_epoch.  Otherwise, local_epoch may be global_epoch or
    // global_epoch - 1.
    void evict(struct way *way, bool local_epoch_is_exact);

    // Flush this core's refcache.
    void flush();

    // Scan this core's review list.  The calling thread must be
    // pinned (but interrupts may be enabled).  At most one review
    // call may be active at a time per core.
    void review();

  public:
    cache() = default;
    cache(const cache &o) = delete;
    cache(cache &&o) = delete;
    cache &operator=(const cache &o) = delete;
    cache &operator=(cache &&o) = delete;

    // Periodic tick handler.  Flushes the refcache and scans review
    // lists.  The latency of garbage collection is between two and
    // three times the delay between calls to tic.
    void tick();

    // Reap dead objects.  This is done in a dedicated thread to
    // avoid deadlock with threads preempted by the timer interrupt.
    void reaper() __attribute__((noreturn));
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

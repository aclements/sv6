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
// We can extend this approach to support "weak references", which
// provide a controlled way to access an object without preventing its
// reference count from reaching zero.  This is useful in situations
// like caches, where the cache needs to reference objects while still
// allowing them to be garbage collected.  A caller can convert a weak
// reference into a regular reference; this will simply fail if the
// referenced object has already been garbage collected.  A weak
// reference is simply a regular pointer plus a "dying" bit.  When an
// object's global reference count initially reaches zero, refcache
// marks the weak reference dying.  After this, the weak reference can
// either be "revived" when a caller converts it to a regular
// reference by clearing the dying bit, or it can be garbage collected
// after the review process clears both its dying bit and its pointer.
// In a race, which succeeds is determined by which clears the dying
// bit first.
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
//         if object.weakref
//           object.weakref.dying <- true
//       else:
//         object.dirty <- true
//
//   review():
//     for each (object, oepoch) in local review queue:
//       if oepoch <= epoch + 2:
//         remove object from the review queue
//         if object.refcnt = 0:
//           if object.dirty:
//             evict(object, 0)
//           else if object.weakref and not object.weakref.dying:
//             evict(object, 0)
//           else:
//             if object.weakref:
//               object.weakref.pointer <- null
//               object.weakref.dying <- false
//             free object
//         else:
//           if object.weakref:
//             object.weakref.dying <- false
//
//   get_weakref(weakref):
//     weakref.dying <- false
//     if weakref.pointer:
//       inc(weakref.pointer)
//     return weakref.pointer
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
#include "spercpu.hh"
#include "kstats.hh"
#include "seqlock.hh"
#include "condvar.h"
#include "critical.hh"

#include <stdexcept>
#include <limits.h>

#ifndef REFCACHE_DEBUG
#define REFCACHE_DEBUG 1
#endif

namespace refcache {
  enum {
    CACHE_SLOTS = 4096
  };

  template<class T> class weakref;

  // Base class for an object that's reference counted using the
  // refcaching scheme.
  class referenced
  {
  private:
    friend class cache;
    template<class T> friend class weakref;

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

    // Seqlock for the refcount value.
    seqcount<uint32_t> refcount_seq_;

    // Link used to track this object in the per-core review list.
    islink<referenced> next_;
    typedef isqueue<referenced, &referenced::next_> list;

    // If this object is on a review list, the epoch in which this
    // object can be reviewed.  0 if this object is not on a review
    // list.
    uint64_t review_epoch_;

    // True if this object's refcount was non-zero and then zero again
    // since it was last reviewed.
    bool dirty_ : 1;

    // True if this object is weak_referenced and may have a weak
    // reference.
    bool weak_ : 1;

  public:
    constexpr referenced(uint64_t refcount = 1)
      : lock_("refcache::referenced"),
        refcount_(refcount),
        next_(),
        review_epoch_(0),
        dirty_(false),
        weak_(false) { }

    referenced(const referenced &o) = delete;
    referenced(referenced &&o) = delete;
    referenced &operator=(const referenced &o) = delete;
    referenced &operator=(referenced &&o) = delete;

    void inc();
    void dec();
    u64 get_consistent();

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

    virtual void onzero() = 0;
  };

  // A subclass of referenced for objects that support a weak
  // reference.  A weak_referenced object can have at most one weak
  // reference to it.
  class weak_referenced : public referenced
  {
    friend class cache;
    template<class T> friend class weakref;

  protected:
    // The weak reference to this object, or nullptr.  We never call
    // get() through this, so it doesn't actually matter what type we
    // use.  Protected (and not private) for weakcache'd objects.
    weakref<weak_referenced> *weakref_;

  public:
    constexpr weak_referenced(uint64_t refcount = 1)
      : referenced(refcount),
        weakref_(nullptr) { }
  };

  // A weak reference to an object of type T, which must be a subclass
  // of weak_referenced.  Unlike a regular reference, a weak reference
  // does not prevent an object's reference count from reaching zero.
  // Prior to an object's onzero method being called to collect the
  // object, any weak reference to it will be set to null.
  //
  // Weak references are designed so that the memory of a referenced
  // object will never be accessed unless we can guarantee that it
  // hasn't been garbage collected and that we can prevent garbage
  // collection by increasing its reference count.  As a result, the
  // referenced object can be deleted immediately in its onzero
  // method, without the need for type-safe memory or delayed freeing
  // techniques (there may, of course, be other constraints on the
  // caller that do require such techniques).
  template<class T>
  class weakref
  {
    static_assert(std::is_base_of<weak_referenced, T>::value,
                  "in weakref<T>, T must be a subclass of weak_referenced");

    friend class cache;

    // Internally, a weakref is a pointer and a single bit "dying"
    // state.  This dying bit is what synchronizes reviving a weak
    // reference and garbage collecting an object: for a dying
    // weakref, either ::get() will successfully revive the pointer by
    // clearing the dying bit (keeping the same pointer value) or
    // garbage collection will successfully clear the dying bit (while
    // also clearing the pointer value).
    struct ptr_and_state
    {
      T* ptr_;
      bool dying_;
      enum { STATE_MASK = 1 };

      ptr_and_state(uintptr_t packed)
        : ptr_((T*)(packed & ~(uintptr_t)STATE_MASK)),
          dying_(packed & STATE_MASK)
      { }

      ptr_and_state(T* ptr, bool dying)
        : ptr_(ptr), dying_(dying)
      { }

      operator uintptr_t() const
      {
        return (uintptr_t)ptr_ | (dying_ ? 1 : 0);
      }
    };

    mutable atomic<uintptr_t> ptr_and_state_;

    // Mark this weak reference as dying to indicate that the
    // referenced object is on a core's review queue.
    void mark_dying(bool dying = true)
    {
      if (dying) {
        if (REFCACHE_DEBUG) {
          // The weakref should only be marked dying by the core that
          // detects the initial zero and puts the object on its
          // review queue.  It could be marked dying again later, but
          // only after it's been revived.
          assert(!ptr_and_state(ptr_and_state_.load()).dying_);
        }
        // XXX Would weaker ordering suffice?
        ptr_and_state_.fetch_or(1);
      } else {
        // (We can't assert that it's not dying here, because it could
        // have been revived in addition to being disowned.)
        ptr_and_state_.fetch_and(~(uintptr_t)1);
      }
    }

    // Compare-exchange from a dying pointer to obj into a null
    // pointer.  This will fail if this weak reference doesn't point
    // to obj or isn't dying (e.g., because it has been revived).
    bool try_break(T* obj)
    {
      uintptr_t expected = ptr_and_state(obj, true);
      return ptr_and_state_.compare_exchange_strong(expected, 0);
    }

  public:
    // Create a null weakref.
    constexpr weakref() : ptr_and_state_() { }

    // Create a weakref initialized to point to ptr.  ptr must have a
    // non-zero reference count and must not have any existing
    // weakref.
    // XXX Should this take an sref<T>?
    weakref(T *ptr = nullptr) : ptr_and_state_((uintptr_t)ptr)
    {
      // Register the back-pointer
      scoped_acquire l(&ptr->lock_);
      if (ptr->review_epoch_)
        // ptr is already on a review list, so this pointer needs to
        // be marked dirty.  While it's an error to create a weakref
        // to a dead object, refcache can conservatively review an
        // object, so referencing a reviewable object isn't
        // necessarily an error.
        mark_dying();
      if (ptr->weakref_)
        throw std::invalid_argument("object cannot have multiple weakrefs");
      ptr->weakref_ = reinterpret_cast<weakref<weak_referenced>*>(this);
      ptr->weak_ = true;
    }

    weakref(const weakref &o) = delete;
    weakref(weakref &&o) = delete;
    weakref &operator=(weakref &o) = delete;
    weakref &operator=(weakref &&o) = delete;

    // Convert this weak reference into a regular reference.  If the
    // pointed-to object has been collected, this will return sref().
    sref<T> get() const;
  };

  // The reference delta cache.  There is one instance of class cache
  // per core.
  class cache
  {
    friend class referenced;

    struct way
    {
      referenced *obj;
      seqcount<uint32_t> seq;
      int32_t delta;

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

    // Return the way in which a particular object's delta could be stored.
    way *hash_way(referenced *obj)
    {
      // Hash based on Java's HashMap re-hashing function.
      std::uint64_t wayno = (uintptr_t)obj;
      wayno ^= (wayno >> 32) ^ (wayno >> 20) ^ (wayno >> 12);
      wayno ^= (wayno >> 7) ^ (wayno >> 4);
      wayno %= CACHE_SLOTS;
      struct way *way = &ways_[wayno];
      // XXX More associativity.  Since this is in the critical path
      // of every reference operation, perhaps we should do something
      // like hash-rehash caching?  Would require returning multiple
      // candidate ways.
      return way;
    }

    // Place obj in the cache if necessary and return its assigned
    // way.  Interrupts must be disabled.
    way *get_way(referenced *obj)
    {
      struct way *way = hash_way(obj);
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
      // If the delta is getting close to overflowing, evict.
      if (way->delta == INT_MAX || way->delta == INT_MIN)
        evict(way, false);
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
  DECLARE_PERCPU(cache, mycache);

  inline void
  referenced::inc()
  {
    // Disable interrupts to prevent review from running on this core
    // in the middle of us updating the local reference cache.
    scoped_cli cli;
    auto way = mycache->get_way(this);
    auto writer = way->seq.write_begin();
    ++way->delta;
  }

  inline void
  referenced::dec()
  {
    scoped_cli cli;
    auto way = mycache->get_way(this);
    auto writer = way->seq.write_begin();
    --way->delta;
  }

  template<class T>
  sref<T>
  weakref<T>::get() const
  {
    // Disable interrupts to prevent a refcache epoch boundary from
    // occurring between this reviving the pointer and incrementing
    // the reference count.
    scoped_cli cli;

    auto psval = ptr_and_state_.load();
  retry:
    ptr_and_state ps(psval);
    if (ps.dying_) {
      // Revive the pointer
      ptr_and_state ps2 = ps;
      ps2.dying_ = false;
      if (!ptr_and_state_.compare_exchange_weak(psval, ps2))
        goto retry;
    }

    // Return sref (and increment the reference count)
    return sref<T>::newref(ps.ptr_);
  }
}

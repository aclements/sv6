#include "refcache.hh"
#include "proc.hh"
#include "kstream.hh"
#include "atomic.hh"

#include <iterator>

//#define TEST

#ifdef TEST
static console_stream verbose(true);
#else
static console_stream verbose(false);
#endif

percpu<refcache::cache> refcache::mycache;

namespace refcache {
  // The current global epoch.  All local epochs are <= the global
  // epoch at all times.  Specifically, because we wait until all
  // cores reach the global epoch before incrementing it, all local
  // epochs are either global_epoch or global_epoch - 1, a fact we
  // exploit during eviction to approximate global_epoch without
  // having to read it.
  static std::atomic<uint64_t> global_epoch __mpalign__;

  // The number of cores where the local epoch is < global_epoch.
  // Once this reaches zero, it is reset to ncpus and the global_epoch
  // incremented.
  static std::atomic<size_t> global_epoch_left __mpalign__;

  static __padout__ __attribute__((unused));
}

void
refcache::cache::evict(struct refcache::cache::way *way,
                       bool local_epoch_is_exact)
{
  referenced *obj = way->obj;
  auto delta = way->delta;
  if (REFCACHE_DEBUG) {
    assert(obj);
    assert(delta);
  }
  scoped_acquire l(&obj->lock_);
  way->delta = 0;
  if ((obj->refcount_ += delta) == 0) {
    // The global count has dropped to zero.  Does this object have a
    // reviewer?
    if (obj->review_epoch_ == 0) {
      // It does not have a reviewer, so put it in our review list.
      verbose.println("refcache: CPU ", myid(), " owning obj ", obj,
                      " with delta ", delta);
      // We have to ensure that all cores flush their refcache between
      // now and when we review this object, so we can review this
      // object as of epoch global_epoch + 2.  Why?  Suppose there are
      // two cores, and right now core 0 has reached global_epoch and
      // core 1 has not.  Reaching global_epoch + 1 is only enough to
      // ensure that *some* cores have flushed: core 1 could flush,
      // reach global_epoch, and increment global_epoch, but this
      // doesn't ensure that core 0 has flushed.  Reaching
      // global_epoch + 2, however, is enough to ensure that all cores
      // have flushed at least once.
      //
      // However, we don't want to read global_epoch, so we
      // approximate it with local_epoch, which may be one less than
      // global_epoch if !local_epoch_is_exact.
      obj->review_epoch_ = local_epoch + (local_epoch_is_exact ? 2 : 3);
      obj->dirty_ = false;
      review_.push_back(obj);
    } else {
      // The object has a reviewer, which means the reviewer needs
      // to know that, even though it's zero again now, it was
      // non-zero during the round and hence unstable.
      verbose.println("refcache: CPU ", myid(), " dirtying obj ", obj,
                      " with delta ", delta);
      obj->dirty_ = true;
    }
  } else {
    verbose.println("refcache: CPU ", myid(), " evicting obj ", obj,
                    " with delta ", delta);
  }
}

void
refcache::cache::review()
{
  // Scan our review list for objects that can be reviewed.  Since we
  // may have interrupts enabled, first find the cut-off.
  uint64_t epoch = global_epoch;
  referenced *last_reviewable = nullptr;
  for (referenced &obj : review_) {
    if (REFCACHE_DEBUG) {
      if (!(obj.review_epoch_ <= epoch + 3))
        spanic.println("refcache: obj.review_epoch_ = ", obj.review_epoch_,
                       ", epoch = ", epoch);
      if (last_reviewable && last_reviewable->review_epoch_ > obj.review_epoch_)
        spanic.println("refcache: out-of-order review list ",
                       last_reviewable->review_epoch_, " > ", obj.review_epoch_);
    }
    if (obj.review_epoch_ > epoch)
      break;
    last_reviewable = &obj;
  }

  if (!last_reviewable)
    return;

  // Cut the review list
  referenced::list reviewable;
  {
    scoped_cli cli;
    reviewable = std::move(review_);
    review_ = reviewable.cut_after(reviewable.iterator_to(last_reviewable));
  }

  // Scan reviewable objects.  Objects will either be deleted,
  // re-added to the review list, or dropped from the review list.
  auto review = reviewable.begin();
  auto review_end = reviewable.end();
  uint64_t nreviewed = 0, nfreed = 0, nrequeued = 0, ndisowned = 0;
  while (review != review_end) {
    auto obj = review++;
    ++nreviewed;
    scoped_acquire l(&obj->lock_);
    if (obj->refcount_ == 0) {
      // The global count was zero at the last epoch and is zero now.
      // Was it non-zero in the meantime?
      if (obj->dirty_) {
        // The reference count was modified during this round, which
        // means it isn't yet stable.  Since it's zero again, it
        // might *now* be stable.  Check it again in another round.
        verbose.println("refcache: CPU ", myid(), " re-queueing obj ", &*obj);
        obj->dirty_ = false;
        obj->review_epoch_ = epoch + 2;
        scoped_cli cli;
        review_.push_back(&*obj);
        ++nrequeued;
      } else {
        // It was zero for the whole round.  Free it.
        verbose.println("refcache: CPU ", myid(), " freeing obj ", &*obj);
        obj->review_epoch_ = 0;
        l.release();
        obj->onzero();
        ++nfreed;
      }
    } else {
      // The count is now non-zero and hence clearly unstable.  Drop
      // it from our review list.  When it goes zero again, some
      // other core will put it on their review list.
      verbose.println("refcache: CPU ", myid(), " disowning obj ", &*obj);
      obj->review_epoch_ = 0;
      ++ndisowned;
    }
  }
  // if (nreviewed)
  //   console.println("refcache: CPU ", myid(), " reviewed ", nreviewed,
  //                   " freed ", nfreed, " requeued ", nrequeued,
  //                   " disowned ", ndisowned);
}

void
refcache::cache::flush()
{
  // XXX Only around flushing?  Depend how flush gets called.
  scoped_cli cli;

  uint64_t cur_global = global_epoch;
  if (cur_global == local_epoch) {
    // We've already reached the global epoch.  There's no point in
    // flushing the cache, since it won't help any core progress in
    // its review list, and we must not join the current global epoch
    // a second time or we'll screw up global_epoch_left.
    return;
  }
  // Update local_epoch so we can tell evict that local_epoch is
  // exact.
  local_epoch = cur_global;

  // Flush our cache
  // XXX Even though we're pinned, we still need interrupts disabled
  // around the evict to avoid preemptions, but we could
  // periodically re-enable them during the loop.
  // XXX This can blow through our CPU cache.  Should we keep a
  // summary bitmap of CPU cache lines containing non-zero deltas?
  std::size_t nflushed = 0;
  for (std::size_t i = 0; i < CACHE_SLOTS; ++i) {
    // Since we have the token now, we can put things directly on
    // the review list for next round because we know that we'll
    // have passed through all of the cores when we next get the
    // token.
    if (ways_[i].delta) {
      evict(&ways_[i], true);
      ++nflushed;
    }
  }
  // if (nflushed)
  //   console.println("refcache: CPU ", myid(), " flushed ", nflushed);

  // Announce that we've reached the global epoch
  if (--global_epoch_left == 0) {
    // We're the last core to reach the global epoch.  Move to the
    // next epoch.
    global_epoch_left = ncpu;
    ++global_epoch;
  }
}

void
refcache::cache::tick()
{
  flush();
  review();
}

#ifdef TEST
class reftest : public refcache::referenced
{
public:
  void onzero()
  {
    verbose.println("refcache: TEST onzero");
  }
};

static void
test(void *)
{
  static reftest rt;
  for (int i = 0; i < 100; i++) {
    myproc()->set_cpu_pin(i % ncpu);
    if (i % 2 == 0)
      rt.inc();
    else
      rt.dec();
  }
  rt.dec();
}
#endif

void
initrefcache(void)
{
  // We use referenced::review_epoch_ == 0 to indicate that there is
  // no reviewer, so start the global epoch count at 1.
  refcache::global_epoch = 1;
  refcache::global_epoch_left = ncpu;

#ifdef TEST
  threadpin(test, nullptr, "refcache test", 0);
#endif
}

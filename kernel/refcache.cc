#include "refcache.hh"
#include "proc.hh"
#include "kstream.hh"

//#define TEST

#ifdef TEST
static console_stream verbose(true);
#else
static console_stream verbose(false);
#endif

percpu<refcache::cache> refcache::mycache;

void
refcache::cache::evict(struct refcache::cache::way *way,
                       refcache::referenced::list *list)
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
    if (!obj->has_reviewer_) {
      // It does not have a reviewer, so put it in our review list.
      // We'll return to this either at the end of this round (if
      // called from get_way because of a capacity eviction) or on
      // the next round (if called from review for a periodic
      // eviction).  Either way, by the time we review this object,
      // the token will have passed through every other core, so
      // we'll know if this refcount is zero for good or not.
      verbose.println("refcache: CPU ", myid(), " owning obj ", obj,
                      " with delta ", delta);
      list->push_front(obj);
      obj->dirty_ = false;
      obj->has_reviewer_ = true;
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
  // Scan our review list.  Objects will either be deleted, put on
  // the new next list, or dropped from the review list.  We have
  // interrupts enabled at this point, so we build up our new review
  // list locally before joining it into next_head_.
  referenced::list::iterator review = review_.begin();
  referenced::list::iterator review_end = review_.end();
  referenced::list new_review;
  referenced::list::iterator new_review_last;
  while (review != review_end) {
    referenced::list::iterator obj = review++;
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
        new_review.push_front(&*obj);
        if (!new_review_last.elem)
          new_review_last = obj;
      } else {
        // It was zero for the whole round.  Free it.
        verbose.println("refcache: CPU ", myid(), " freeing obj ", &*obj);
        obj->has_reviewer_ = false;
        l.release();
        obj->onzero();
      }
    } else {
      // The count is now non-zero and hence clearly unstable.  Drop
      // it from our review list.  When it goes zero again, some
      // other core will put it on their review list.
      verbose.println("refcache: CPU ", myid(), " disowning obj ", &*obj);
      obj->has_reviewer_ = false;
    }
  }

  // Merge the next list built above with our current next list
  // (which may be non-empty if there were capacity evictions during
  // the last round) and make it the review list for next round.
  pushcli();
  if (new_review_last.elem) {
    new_review.splice_after(new_review_last, std::move(next_));
    review_ = std::move(new_review);
  } else {
    review_ = std::move(next_);
    next_.clear();
  }

  // Flush our cache
  // XXX Even though we're pinned, we still need interrupts disabled
  // around the evict to avoid preemptions, but we could
  // periodically re-enable them during the loop.
  // XXX This can blow through our CPU cache.  Should we keep a
  // summary bitmap of CPU cache lines containing non-zero deltas?
  std::size_t flushed = 0;
  for (std::size_t i = 0; i < CACHE_SLOTS; ++i) {
    // Since we have the token now, we can put things directly on
    // the review list for next round because we know that we'll
    // have passed through all of the cores when we next get the
    // token.
    if (ways_[i].delta) {
      evict(&ways_[i], &review_);
      ++flushed;
    }
  }
//  if (flushed)
//    verbose.println("refcache: CPU ", myid(), " flushed ", flushed, " slots");
  popcli();
}

void
refcache::cache::worker(void *)
{
  // XXX Should this delay, or wait until hash tables are full enough,
  // or wait until review lists are long, or what?
  while (true) {
    // XXX This migration scheme is simple, but requires more cache
    // line movement than simply signaling each core in turn.
    for (int i = 0; i < ncpu; ++i) {
      myproc()->set_cpu_pin(i);
      // Percpu is safe because we're pinned
      mycache.get_unchecked()->review();
    }
  }
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
  threadpin(refcache::cache::worker, nullptr, "refcache", 0);
#ifdef TEST
  threadpin(test, nullptr, "refcache test", 0);
#endif
}

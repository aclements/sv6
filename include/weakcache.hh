#pragma once

#include "gc.hh"
#include "spinlock.hh"
#include "refcache.hh"
#include "hash.hh"
#include "log2.hh"

template<class K, class V>
class weakcache
{
public:
  struct stats
  {
    size_t items;
    size_t total_buckets, used_buckets;
    size_t max_chain;
  };

private:
  class bucket;

  class item : public rcu_freed
  {
  public:
    const K key_;
    const refcache::weakref<V> weakref_;
    bucket* const parent_;
    islink<item> link_;

    item(const K& k, V* v, bucket* b)
      : rcu_freed("weakcache::item", this, sizeof(*this)),
        key_(k), weakref_(v), parent_(b) {}
    void do_gc() override { delete this; }
    NEW_DELETE_OPS(item)
  };

  class bucket
  {
  private:
    spinlock writelock_;
    islist<item, &item::link_> chain_;

  public:
    sref<V>
    lookup(const K& k) const
    {
      scoped_gc_epoch reader;
      for (auto &i: chain_) {
        if (!(i.key_ == k))
          continue;
        return i.weakref_.get();
      }
      return sref<V>();
    }

    bool
    insert(const K& k, V* v)
    {
      scoped_acquire l(&writelock_);
      for (auto &i: chain_)
        if (i.key_ == k)
          return false;

      chain_.push_front(new item(k, v, this));
      return true;
    }

    void
    remove(item* victim)
    {
      scoped_acquire l(&writelock_);
      auto i = chain_.before_begin();
      auto end = chain_.end();
      for (;;) {
        auto prev = i;
        ++i;
        assert(i != end);
        if (&*i == victim) {
          chain_.erase_after(prev);
          gc_delayed(victim);
          return;
        }
      }
    }

    void
    update_stats(struct stats *stats) const
    {
      size_t my_items = 0;
      for (auto it = chain_.begin(), end = chain_.end(); it != end; ++it)
        ++my_items;
      stats->items += my_items;
      if (my_items)
        stats->used_buckets += 1;
      if (my_items > stats->max_chain)
        stats->max_chain = my_items;
    }
  };

  const uintptr_t mask_;
  bucket *buckets_;

public:
  // Construct a weak cache whose bucket array fits in size bytes.
  // This must be called before initkalloc since it requires large,
  // raw allocations from the boot allocator.
  weakcache(std::size_t size)
    : mask_(round_down_to_pow2(size / sizeof *buckets_) - 1)
  {
    buckets_ = (bucket*)early_kalloc((mask_ + 1) * sizeof *buckets_, PGSIZE);
    if (!buckets_)
      throw_bad_alloc();
    new (buckets_) bucket[mask_ + 1];
  }

  ~weakcache()
  {
    panic("weakcache::~weakcache");
  }

  weakcache(const weakcache &o) = delete;
  weakcache(weakcache &&o) = delete;
  weakcache &operator=(const weakcache &o) = delete;
  weakcache &operator=(weakcache &&o) = delete;

  sref<V>
  lookup(const K& k) const
  {
    return buckets_[hash(k) & mask_].lookup(k);
  }

  bool
  insert(const K& k, V* v)
  {
    return buckets_[hash(k) & mask_].insert(k, v);
  }

  void
  cleanup(refcache::weakref<refcache::weak_referenced>* refp)
  {
    refcache::weakref<V>* vrefp = reinterpret_cast<refcache::weakref<V>*>(refp);
    item* i = container_from_member(vrefp, &item::weakref_);
    i->parent_->remove(i);
  }

  struct stats
  get_stats() const
  {
    struct stats res{};
    res.total_buckets = mask_ + 1;
    for (std::size_t i = 0; i < mask_ + 1; ++i)
      buckets_[i].update_stats(&res);
    return res;
  };
};

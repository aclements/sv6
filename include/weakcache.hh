#pragma once

#include "gc.hh"
#include "spinlock.h"
#include "refcache.hh"
#include "hash.hh"

template<class K, class V, size_t Buckets>
class weakcache
{
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
  };

  bucket buckets_[Buckets];

public:
  sref<V>
  lookup(const K& k) const
  {
    return buckets_[hash(k) % Buckets].lookup(k);
  }

  bool
  insert(const K& k, V* v)
  {
    return buckets_[hash(k) % Buckets].insert(k, v);
  }

  void
  cleanup(refcache::weakref<refcache::weak_referenced>* refp)
  {
    refcache::weakref<V>* vrefp = reinterpret_cast<refcache::weakref<V>*>(refp);
    item* i = container_from_member(vrefp, &item::weakref_);
    i->parent_->remove(i);
  }
};

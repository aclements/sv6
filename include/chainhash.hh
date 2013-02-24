#pragma once

/*
 * A bucket-chaining hash table.
 */

#include "spinlock.h"
#include "seqlock.hh"
#include "lockwrap.hh"
#include "hash.hh"
#include "ilist.hh"

template<class K, class V>
class chainhash {
private:
  struct item : public rcu_freed {
    item(const K& k, const V& v)
      : rcu_freed("chainhash::item", this, sizeof(*this)),
        key(k), val(v) {}
    void do_gc() override { delete this; }
    NEW_DELETE_OPS(item);

    islink<item> link;
    seqcount<u32> seq;
    const K key;
    V val;
  };

  struct bucket {
    spinlock lock;
    islist<item, &item::link> chain;

    ~bucket() {
      while (!chain.empty()) {
        item *i = &chain.front();
        chain.pop_front();
        gc_delayed(i);
      }
    }
  };

  u64 nbuckets_;
  bool dead_;
  bucket* buckets_;

public:
  chainhash(u64 nbuckets) : nbuckets_(nbuckets), dead_(false) {
    buckets_ = new bucket[nbuckets_];
    assert(buckets_);
  }

  ~chainhash() {
    delete[] buckets_;
  }

  NEW_DELETE_OPS(chainhash);

  bool insert(const K& k, const V& v) {
    if (dead_ || lookup(k))
      return false;

    bucket* b = &buckets_[hash(k) % nbuckets_];
    scoped_acquire l(&b->lock);

    if (dead_)
      return false;

    for (const item& i: b->chain)
      if (i.key == k)
        return false;

    b->chain.push_front(new item(k, v));
    return true;
  }

  bool remove(const K& k, const V& v) {
    if (!lookup(k))
      return false;

    bucket* b = &buckets_[hash(k) % nbuckets_];
    scoped_acquire l(&b->lock);

    auto i = b->chain.before_begin();
    auto end = b->chain.end();
    for (;;) {
      auto prev = i;
      ++i;
      if (i == end)
        return false;
      if (i->key == k && i->val == v) {
        b->chain.erase_after(prev);
        gc_delayed(&*i);
        return true;
      }
    }
  }

  bool replace(const K& k, const V& vold, const V& vnew) {
    if (!lookup(k))
      return false;

    bucket* b = &buckets_[hash(k) % nbuckets_];
    scoped_acquire l(&b->lock);

    for (item& i: b->chain) {
      if (i.key == k) {
        if (i.val != vold)
          return false;
        auto w = i.seq.write_begin(); 
        i.val = vnew;
        return true;
      }
    }

    return false;
  }

  bool enumerate(const K* prev, K* out) const {
    scoped_gc_epoch rcu_read;

    bool prevbucket = (prev != nullptr);
    for (u64 i = prev ? hash(*prev) % nbuckets_ : 0; i < nbuckets_; i++) {
      bucket* b = &buckets_[i];
      bool found = false;
      for (const item& i: b->chain) {
        if ((!prevbucket || *prev < i.key) && (!found || i.key < *out)) {
          *out = i.key;
          found = true;
        }
      }
      if (found)
        return true;
      prevbucket = false;
    }

    return false;
  }

  bool lookup(const K& k, V* vptr = nullptr) const {
    scoped_gc_epoch rcu_read;

    bucket* b = &buckets_[hash(k) % nbuckets_];
    for (const item& i: b->chain) {
      if (i.key != k)
        continue;
      if (vptr)
        *vptr = *seq_reader<V>(&i.val, &i.seq);
      return true;
    }
    return false;
  }

  bool remove_and_kill(const K& k, const V& v) {
    if (dead_)
      return false;

    for (u64 i = 0; i < nbuckets_; i++)
      for (const item& ii: buckets_[i].chain)
        if (ii.key != k || ii.val != v)
          return false;

    for (u64 i = 0; i < nbuckets_; i++)
      buckets_[i].lock.acquire();

    bool killed = !dead_;
    for (u64 i = 0; i < nbuckets_; i++)
      for (const item& ii: buckets_[i].chain)
        if (ii.key != k || ii.val != v)
          killed = false;

    if (killed) {
      dead_ = true;
      bucket* b = &buckets_[hash(k) % nbuckets_];
      item* i = &b->chain.front();
      assert(i->key == k && i->val == v);
      b->chain.pop_front();
      gc_delayed(i);
    }

    for (u64 i = 0; i < nbuckets_; i++)
      buckets_[i].lock.release();

    return killed;
  }

  bool killed() const {
    return dead_;
  }
};

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
    item(const K& k, const V& v) : rcu_freed("chainhash::item"), key(k), val(v) {}
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
  bucket* buckets_;

public:
  chainhash(u64 nbuckets) : nbuckets_(nbuckets) {
    buckets_ = new bucket[nbuckets_];
    assert(buckets_);
  }

  ~chainhash() {
    delete[] buckets_;
  }

  NEW_DELETE_OPS(chainhash);

  bool insert(const K& k, const V& v) {
    bucket* b = &buckets_[hash(k) % nbuckets_];
    scoped_acquire l(&b->lock);

    for (const item& i: b->chain)
      if (i.key == k)
        return false;

    b->chain.push_front(new item(k, v));
    return true;
  }

  bool remove(const K& k, V* vptr) {
    bucket* b = &buckets_[hash(k) % nbuckets_];
    scoped_acquire l(&b->lock);

    auto i = b->chain.before_begin();
    auto end = b->chain.end();
    for (;;) {
      auto prev = i;
      ++i;
      if (i == end)
        return false;
      if (i->key == k) {
        b->chain.erase_after(prev);
        *vptr = i->val;
        gc_delayed(&*i);
        return true;
      }
    }
  }

  bool lookup(const K& k, V* vptr) const {
    bucket* b = &buckets_[hash(k) % nbuckets_];
    for (const item& i: b->chain) {
      if (i.key != k)
        continue;
      *vptr = *seq_reader<V>(&i.val, &i.seq);
      return true;
    }
    return false;
  }
};

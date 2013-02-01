#pragma once

#include "sleeplock.hh"
#include "ns.hh"

namespace uniqstate {
  enum {
    EMBRYO,
    LIVE,
    REVIVING,
    DYING,
    DEAD
  };
}

template<class Item>
class uniqcache;

template<class KeyType, class ItemType>
class uniqitem : public rcu_freed {
public:
  typedef KeyType key_t;

  uniqitem(const key_t& k, uniqcache<ItemType>* c)
    : rcu_freed("uniqitem"), key_(k), cache_(c) {}

  // Provided functions
  void evict();   // Try to evict this item
  void do_gc();

  // Functions to override
  static u64 keyhash(const key_t& k);
  void load();    // Load into cache
  bool dirty();   // Is this item dirty (non-evictable)?

  // Internal state for a cached item
  const KeyType key_;
  sleeplock write_lock_;

private:
  uniqcache<ItemType>* const cache_;   // for evict's gc_delayed
  std::atomic<u8> state_;

  friend class uniqcache<ItemType>;
};

// Item must publicly inherit from uniqitem.
template<class Item>
class uniqcache
{
private:
  typedef typename Item::key_t key_t;
  xns<key_t, Item*, Item::keyhash> hashtable_;

  friend class uniqitem<key_t, Item>;

public:
  uniqcache() : hashtable_(false) {}
  NEW_DELETE_OPS(uniqcache);

  // Must be called from within an RCU read section,
  // to ensure the returned Item is not evicted.
  Item* get(const key_t& k);
};

template<class Item>
Item*
uniqcache<Item>::get(const key_t& k)
{
retry_lookup:
  Item* i = hashtable_.lookup(k);
  if (!i) {
    i = new Item(k, this);
    i->state_ = uniqstate::EMBRYO;
    i->write_lock_.acquire();

    if (!hashtable_.insert(k, i)) {
      delete i;
      goto retry_lookup;
    }

    i->load();
    i->state_ = uniqstate::LIVE;
    i->write_lock_.release();
  }

  if (i->state_ == uniqstate::EMBRYO) {
    // Wait for the load()er to release the lock
    i->write_lock_.acquire();
    i->write_lock_.release();
    assert(i->state_ != uniqstate::EMBRYO);
  }

dead_check:
  u8 s = i->state_.load();
  if (s == uniqstate::DEAD)
    goto retry_lookup;

  if (s == uniqstate::DYING) {
    if (!cmpxch(&i->state_, (u8) uniqstate::DYING,
                            (u8) uniqstate::REVIVING))
      goto dead_check;
  }

  return i;
}

template<class KeyType, class ItemType>
void
uniqitem<KeyType, ItemType>::evict()
{
  if (!cmpxch(&state_, (u8) uniqstate::LIVE,
                       (u8) uniqstate::DYING))
    return;

  gc_delayed(this);
}

template<class KeyType, class ItemType>
void
uniqitem<KeyType, ItemType>::do_gc()
{
  ItemType* thisitem = (ItemType*) this;

  u8 s = state_.load();
  if (s == uniqstate::DEAD) {
    delete this;
    return;
  }

  if (thisitem->dirty()) {
    state_ = uniqstate::LIVE;
    return;
  }

  if (!cmpxch(&state_, (u8) uniqstate::DYING,
                       (u8) uniqstate::DEAD)) {
    assert(state_ == (u8) uniqstate::REVIVING);
    state_ = uniqstate::LIVE;
    return;
  }

  assert(cache_->hashtable_.remove(key_, &thisitem));
  gc_delayed(this);
}

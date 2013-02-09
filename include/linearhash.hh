#pragma once

/*
 * A linear-probing hash table.
 */

#include "spinlock.h"
#include "seqlock.hh"
#include "lockwrap.hh"
#include "hash.hh"

template<class K, class V>
class linearhash {
private:
  struct slotdata {
    slotdata() : used(false), valid(false) {}

    bool used;
    bool valid;
    K key;
    V val;
  };

  struct slot {
    spinlock lock;
    seqcount<u32> seq;
    slotdata data;
  };

  u64 nslots_;
  slot* slots_;

public:
  linearhash(u64 nslots) : nslots_(nslots) {
    slots_ = new slot[nslots_];
    assert(slots_);
  }

  ~linearhash() {
    delete[] slots_;
  }

  NEW_DELETE_OPS(linearhash);

  void insert(const K& k, const V& v) {
    u64 h = hash(k);
    for (u64 i = 0; i < nslots_; i++) {
      slot* s = &slots_[(h + i) % nslots_];
      scoped_acquire l(&s->lock);
      if (s->data.valid)
        continue;

      auto w = s->seq.write_begin();
      s->data.used = true;
      s->data.valid = true;
      s->data.key = k;
      s->data.val = v;
      return;
    }

    panic("insert: out of slots");
  }

  bool remove(const K& k) {
    u64 h = hash(k);
    for (u64 i = 0; i < nslots_; i++) {
      slot* s = &slots_[(h + i) % nslots_];
      scoped_acquire l(&s->lock);
      if (!s->data.used)
        break;
      if (s->data.key == k) {
        auto w = s->seq.write_begin();
        s->data.valid = false;
        return true;
      }
    }
    return false;
  }

  bool lookup(const K& k, V* vptr) const {
    u64 h = hash(k);
    for (u64 i = 0; i < nslots_; i++) {
      const slot* s = &slots_[(h + i) % nslots_];
      auto copy = seq_reader<slotdata>(&s->data, &s->seq);
      if (!copy->used)
        break;
      if (copy->valid && copy->key == k) {
        *vptr = copy->val;
        return true;
      }
    }
    return false;
  }
};

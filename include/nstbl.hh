#pragma once

#define NINS      251

// Prevent potentially non-scalable bucket groupings
static_assert(NINS != NHASH, "Bad NINS choice");

template<class K, class V, u64 (*HF)(const K&)>
class nstbl : public rcu_freed
{
  typedef xns<K, V, HF> ins;

public:
  nstbl() : rcu_freed("nstbl", this, sizeof(*this)) {
    for (int i = 0; i < NINS; i++)
      a_[i].store(nullptr);
  }

  ~nstbl() {
    for (int i = 0; i < NINS; i++)
      if (a_[i].load() != nullptr) {
        delete a_[i].load();
        a_[i].store(nullptr);
      }
  }

  bool remove(const K& key, const V* vp = nullptr) {
    u32 i = ah(key);
    auto n = a_[i].load();
    if (n != nullptr)
      return n->remove(key, vp);
    return false;
  }

  bool replace(const K& key, const V& oldval, const V& newval) {
    u32 i = ah(key);
    auto n = a_[i].load();
    if (n != nullptr)
      return n->replace(key, oldval, newval);
    return false;
  }

  bool insert(const K& key, const V& val) {
    ins* ns;
    u32 i;

    i = ah(key);

  retry:
    if (a_[i].load() == nullptr) {
      ns = new ins(false);
      if (!cmpxch(&a_[i], (decltype(ns)) nullptr, ns)) {
        delete ns;
        goto retry;
      }
    } else {
      ns = a_[i];
    }
    
    return ns->insert(key, val);
  }

  V lookup(const K& key) const {
    ins* ns;
    u32 i;

    i = ah(key);
    ns = a_[i].load();
    if (ns != nullptr)
      return ns->lookup(key);
    return 0;
  }

  template<class CB>
  void enumerate(CB cb) const {
    // XXX(sbw) not the same semantics as xns::enumerate.
    // This continues if cb returns true.
    for (int i = 0; i < NINS; i++) {
      ins* ns = a_[i].load();
      if (ns != nullptr)
        ns->enumerate(cb);
    }
  }

  void do_gc(void) override {
    delete this;
  }

  NEW_DELETE_OPS(nstbl);

private:
  std::atomic<ins*> a_[NINS];

  u32 ah(const K& key) const {
    u64 h = HF(key);
    return h % NINS;
  }
};

#pragma once

#include "rnd.hh"
#include "percpu.hh"

template<int N>
struct random_permutation {
 public:
  random_permutation() : i_(0), n_(N) {
    assert(n_ <= N && n_ <= 256);
    for (int i = 0; i < n_; i++)
      x_[i] = i;
  }

  void resize(int n) {
    n_ = n;
    for (int i = 0; i < n_; i++)
      x_[i] = i;
  }

  void reset() {
    i_ = 0;
  }

  int next() {
#if CODEX
    int r = rnd() % (n_ - i_);
#else
    int r = rdtsc() % (n_ - i_);
#endif
    std::swap(x_[i_], x_[r]);
    return x_[i_++];
  }

 private:
  char x_[N];
  int i_;
  size_t n_;
};

template<class Pool>
class balance_pool {
 public:
  balance_pool(u64 max) : balance_max_(max) {}

  bool balanced() const {
    Pool* thispool = (Pool*) this;
    u64 c = thispool->balance_count();
    return c != 0 && c != balance_max_;
  }

  void balance_with(Pool* otherpool) {
    Pool* thispool = (Pool*) this;
    u64 thisbal = thispool->balance_count();
    u64 otherbal = otherpool->balance_count();

    if (thisbal < otherbal) {
      otherpool->balance_move_to(thispool);
    } else if (otherbal > thisbal) {
      thispool->balance_move_to(otherpool);
    }
  }

 private:
  u64 balance_max_;
};

template<class PoolDir, class Pool>
class balancer {
 public:
  balancer(const PoolDir* bd) : bd_(bd) {
    assert(ncpu != 0 && nsocket != 0); // make sure ncpu/nsocket have been initialized.
    for(int i = 0; i < ncpu; i++) {
      rpsock_[i].resize(ncpu/nsocket - 1);
      rpother_[i].resize(ncpu - ncpu/nsocket);
    }
  }
  ~balancer() {}

  void balance() {
    int myid = mycpu()->id;
    Pool* thispool = bd_->balance_get(myid);
    if (!thispool)
      return;

    u64 sock_first_core = (myid / (ncpu/nsocket)) * (ncpu/nsocket);
    u64 sock_myoff = myid % (ncpu/nsocket);

    rpsock_->reset();

    for (int i = 0; i < (ncpu/nsocket)-1; i++) {
      int bal_id = sock_first_core +
        ((sock_myoff + 1 + rpsock_->next()) % (ncpu/nsocket));
      Pool* otherpool = bd_->balance_get(bal_id);
      if (otherpool && (thispool != otherpool)) {
        thispool->balance_with(otherpool);
        if (thispool->balanced())
          return;
      }
    }

    rpother_->reset();
    for (int i = 0; i < NCPU-(ncpu/nsocket); i++) {
      int bal_id = (sock_first_core + (ncpu/nsocket) +
                 rpother_->next()) % NCPU;
      Pool* otherpool = bd_->balance_get(bal_id);
      if (otherpool && (thispool != otherpool)) {
        thispool->balance_with(otherpool);
        if (thispool->balanced())
          break;
      }
    }
  }

 private:
  const PoolDir* const bd_;
  percpu<random_permutation<NCPU-1>,NO_CRITICAL> rpsock_;
  percpu<random_permutation<NCPU-1>,NO_CRITICAL> rpother_;
};

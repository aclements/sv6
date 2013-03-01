#pragma once
#define NCPU_PER_SOCKET (NCPU/NSOCKET)

#include "rnd.hh"

template<int N>
struct random_permutation {
 public:
  random_permutation() : i_(0) {
    assert(N <= 256);
    for (int i = 0; i < N; i++)
      x_[i] = i;
  }

  void reset() {
    i_ = 0;
  }

  int next() {
#if CODEX
    int r = rnd() % (N - i_);
#else
    int r = rdtsc() % (N - i_);
#endif
    std::swap(x_[i_], x_[r]);
    return x_[i_++];
  }

 private:
  char x_[N];
  int i_;
};

// For wrapping into <percpu>, which maybe important to avoid false sharing
struct persocket {
  random_permutation<NCPU_PER_SOCKET-1> perm;
};

struct othersocket {
  random_permutation<NCPU-NCPU_PER_SOCKET> perm;
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
  balancer(const PoolDir* bd) : bd_(bd) {}
  ~balancer() {}

  void balance() {
    int myid = mycpu()->id;
    Pool* thispool = bd_->balance_get(myid);
    if (!thispool)
      return;

    u64 sock_first_core = (myid / NCPU_PER_SOCKET) * NCPU_PER_SOCKET;
    u64 sock_myoff = myid % NCPU_PER_SOCKET;

    rpsock_->perm.reset();

    for (int i = 0; i < NCPU_PER_SOCKET-1; i++) {
      int bal_id = sock_first_core +
        ((sock_myoff + 1 + rpsock_->perm.next()) % NCPU_PER_SOCKET);
      Pool* otherpool = bd_->balance_get(bal_id);
      if (otherpool && (thispool != otherpool)) {
        thispool->balance_with(otherpool);
        if (thispool->balanced())
          return;
      }
    }

    rpother_->perm.reset();
    for (int i = 0; i < NCPU-NCPU_PER_SOCKET; i++) {
      int bal_id = (sock_first_core + NCPU_PER_SOCKET +
                 rpother_->perm.next()) % NCPU;
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
  percpu<persocket,NO_CRITICAL> rpsock_;
  percpu<othersocket,NO_CRITICAL> rpother_;
};

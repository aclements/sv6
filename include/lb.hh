#pragma once
#define NCPU_PER_SOCKET (NCPU/NSOCKET)

template<int N>
class random_permutation {
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
    int r = rdtsc() % (N - i_);
    std::swap(x_[i_], x_[r]);
    return x_[i_++];
  }

 private:
  char x_[N];
  int i_;
};

class balance_pool {
 public:
  balance_pool(u64 max) : balance_max_(max) {}

  virtual u64 balance_count() const = 0;

  virtual bool balanced() const {
    u64 c = balance_count();
    return c != 0 && c != balance_max_;
  }

  virtual void balance_with(balance_pool* otherpool) {
    u64 thisbal = balance_count();
    u64 otherbal = otherpool->balance_count();

    if (thisbal < otherbal) {
      otherpool->balance_move_to(this);
    } else {
      balance_move_to(otherpool);
    }
  }

  virtual void balance_move_to(balance_pool* other) = 0;

 private:
  u64 balance_max_;
};

class balance_pool_dir {
 public:
  virtual balance_pool* balance_get(int id) const = 0;
};

class balancer {
 public:
  balancer(const balance_pool_dir* bd) : bd_(bd) {}
  ~balancer() {}

  void balance() {
    int myid = mycpu()->id;
    balance_pool* thispool = bd_->balance_get(myid);
    if (!thispool)
      return;

    u64 sock_first_core = (myid / NCPU_PER_SOCKET) * NCPU_PER_SOCKET;
    u64 sock_myoff = myid % NCPU_PER_SOCKET;

    scoped_acquire l(&rplock_[myid]);
    rpsock_[myid].reset();
    for (int i = 0; i < NCPU_PER_SOCKET-1; i++) {
      int bal_id = sock_first_core +
                   ((sock_myoff + 1 + rpsock_[myid].next()) % NCPU_PER_SOCKET);
      balance_pool* otherpool = bd_->balance_get(bal_id);
      if (otherpool) {
        thispool->balance_with(otherpool);
        if (thispool->balanced())
          return;
      }
    }

    rpother_[myid].reset();
    for (int i = 0; i < NCPU-NCPU_PER_SOCKET; i++) {
      int bal_id = (sock_first_core + NCPU_PER_SOCKET +
                    rpother_[myid].next()) % NCPU;
      balance_pool* otherpool = bd_->balance_get(bal_id);
      if (otherpool) {
        thispool->balance_with(otherpool);
        if (thispool->balanced())
          return;
      }
    }
  }

 private:
  const balance_pool_dir* bd_;
  random_permutation<NCPU_PER_SOCKET-1> rpsock_[NCPU];
  random_permutation<NCPU-NCPU_PER_SOCKET> rpother_[NCPU];
  spinlock rplock_[NCPU];   // protects the per-core random permutation state
};

#pragma once
#define NCPU_PER_SOCKET (NCPU/NSOCKET)

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
    int r = rdtsc() % (N - i_);
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

struct mylock {
  spinlock lx;
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
    } else if (otherbal > thisbal) {
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

    scoped_acquire l(&(rplock_->lx));
    rpsock_->perm.reset();

    for (int i = 0; i < NCPU_PER_SOCKET-1; i++) {
      int bal_id = sock_first_core +
        ((sock_myoff + 1 + rpsock_->perm.next()) % NCPU_PER_SOCKET);
      balance_pool* otherpool = bd_->balance_get(bal_id);
      if (otherpool && (thispool != otherpool)) {
        thispool->balance_with(otherpool);
        if (thispool->balanced())
          return;
      }
    }

    //u64 t0, t1;
    //t0 = rdtsc();
    rpother_->perm.reset();
    for (int i = 0; i < NCPU-NCPU_PER_SOCKET; i++) {
      int bal_id = (sock_first_core + NCPU_PER_SOCKET +
                 rpother_->perm.next()) % NCPU;
      balance_pool* otherpool = bd_->balance_get(bal_id);
      if (otherpool && (thispool != otherpool)) {
        thispool->balance_with(otherpool);
        if (thispool->balanced())
          break;
      }
    }
#if 0
  t1 = rdtsc();
      ++*counter;
      *tot = *tot + (t1-t0);
    
#define N1 100000
    if ((*counter % N1 == 0) && mycpu()->id == 1) {
      cprintf("%d: time %lu\n", mycpu()->id, *tot / N1);
      *tot = 0;
    }
#endif
  }

private:
  percpu<u64> counter;
  percpu<u64> tot;
  
  const balance_pool_dir* bd_;
  percpu<persocket,percpu_safety::internal> rpsock_;
  percpu<othersocket,percpu_safety::internal> rpother_;
  percpu<mylock,percpu_safety::internal> rplock_;   // protects the per-core random permutation state
};

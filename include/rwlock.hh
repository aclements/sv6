#pragma once

#include "spinlock.hh"
#include "condvar.hh"

// NOTE: this is a dead-simple read/write lock implementations.
// do not expect this to be fast; feel free to improve it.

// sleeplock, but read-write lock
class rwlock {
public:
  rwlock() : reader(this), writer(this), readers(0) {}

  class read {
  public:
    void acquire() {
      lock_guard<spinlock> x(&rw->spin);
      while (rw->readers == rwlock_held_for_writing)
        rw->cond.sleep(&rw->spin);
      rw->readers++;
      assert(rw->readers != rwlock_held_for_writing); // overflow check
    }

    bool try_acquire() {
      lock_guard<spinlock> x(&rw->spin);
      if (rw->readers == rwlock_held_for_writing)
        return false;
      rw->readers++;
      assert(rw->readers != rwlock_held_for_writing); // overflow check
      return true;
    }

    void release() {
      lock_guard<spinlock> x(&rw->spin);
      assert(rw->readers != rwlock_held_for_writing);
      assert(rw->readers > 0);
      rw->readers--;
      if (rw->readers == 0)
        rw->cond.wake_all();
    }
  private:
    explicit read(rwlock *rw) : rw(rw) {}
    rwlock *rw;

    friend rwlock;
  };

  class write {
  public:
    void acquire() {
      lock_guard<spinlock> x(&rw->spin);
      while (rw->readers != 0 || rw->upgrading)
        rw->cond.sleep(&rw->spin);
      rw->readers = rwlock_held_for_writing;
    }

    bool try_acquire() {
      lock_guard<spinlock> x(&rw->spin);
      if (rw->readers != 0 || rw->upgrading)
        return false;
      rw->readers = rwlock_held_for_writing;
      return true;
    }

    void release() {
      lock_guard<spinlock> x(&rw->spin);
      assert(rw->readers == rwlock_held_for_writing);
      rw->readers = 0;
      rw->cond.wake_all();
    }
  private:
    explicit write(rwlock *rw) : rw(rw) {}
    rwlock *rw;

    friend rwlock;
  };

  lock_guard<read> guard_read() {
    return lock_guard<read>(&reader);
  }

  lock_guard<write> guard_write() {
    return lock_guard<write>(&writer);
  }

  // an upgrade either succeeds and atomically switches from a reader lock to a writer lock, or fails because another
  // thread tried to upgrade at the exact same time.
  lock_guard<write> upgrade(lock_guard<read> &r) {
    auto o = r.unsafe_transfer_out();
    if (!o)
      return lock_guard<write>();
    assert(o == &reader);

    lock_guard<spinlock> l(&spin);
    assert(readers != rwlock_held_for_writing && readers >= 1);
    readers--; // release the passed-in guard

    // try to claim upgrade lock
    if (upgrading)
      return lock_guard<write>(); // someone else is upgrading; can't do it!
    upgrading = true;
    while (readers > 0) {
      assert(readers != rwlock_held_for_writing); // nobody should be able to take the lock for writing right now
      cond.sleep(&spin);
    }
    assert(upgrading && readers == 0);
    readers = rwlock_held_for_writing;
    upgrading = false;

    lock_guard<write> w;
    w.unsafe_transfer_in(&writer);
    return w;
  }

  // atomically switches from a writer lock to a reader lock
  lock_guard<read> downgrade(lock_guard<write> &w) {
    auto o = w.unsafe_transfer_out();
    if (!o)
      return lock_guard<read>();
    assert(o == &writer);

    lock_guard<spinlock> l(&spin);
    assert(readers == rwlock_held_for_writing);
    assert(!upgrading); // nobody can be upgrading, since we held the write lock
    readers = 1;
    cond.wake_all();

    lock_guard<read> r;
    r.unsafe_transfer_in(&reader);
    return r;
  }

  read reader;
  write writer;
private:
  static constexpr u64 rwlock_held_for_writing = 0xFFFFFFFF;
  spinlock spin;
  condvar cond;
  bool upgrading; // if set, DO NOT acquire the lock except by upgrade
  u64 readers; // small integer or rwlock_held_for_writing
};

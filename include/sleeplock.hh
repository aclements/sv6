#pragma once

#include "spinlock.hh"
#include "condvar.hh"

class sleeplock {
 public:
  sleeplock() : held_(false) {}

  void acquire() {
    scoped_acquire x(&spinlock_);
    while (held_)
      cv_.sleep(&spinlock_);
    held_ = true;
  }

  bool try_acquire() {
    scoped_acquire x(&spinlock_);
    if (held_)
      return false;
    held_ = true;
    return true;
  }

  void release() {
    scoped_acquire x(&spinlock_);
    held_ = false;
    cv_.wake_all();
  }

  lock_guard<sleeplock> guard() {
    return lock_guard<sleeplock>(this);
  }

 private:
  spinlock spinlock_;
  condvar cv_;
  bool held_;
};

#pragma once

#include "sched.hh"
#include "ilist.hh"

// Structures for deferring work.  The deferrred work must not go to block/sleep.
// If it goes to sleep, create a thread and pin it on the desired core.

struct dwork {
  dwork() {}
  virtual void run() = 0;
  islink<dwork> link_;
};

struct dwframe {
  dwframe(int v = 0) : v_(v) {}
  void clear() { v_ = 0; }
  int inc() { return __sync_add_and_fetch(&v_, 1); }
  int dec() { return __sync_sub_and_fetch(&v_, 1); }
  bool zero() volatile { return v_ == 0; };
  volatile int v_;
};

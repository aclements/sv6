#pragma once

#include "sched.hh"

// Structures for deferring work

struct dwork : public work_link {
  dwork() {}
  virtual void run() = 0;
};

struct dwframe {
  dwframe(int v = 0) : v_(v) {}
  void clear() { v_ = 0; }
  int inc() { return __sync_add_and_fetch(&v_, 1); }
  int dec() { return __sync_sub_and_fetch(&v_, 1); }
  bool zero() volatile { return v_ == 0; };
  volatile int v_;
};

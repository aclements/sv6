#pragma once

#include "cpu.hh"
#include "amd64.h"
#include "bits.hh"
#include "spercpu.hh"

template <typename T, percpu_safety S = percpu_safety::cli>
struct percpu {
  constexpr percpu() = default;

  percpu(const percpu &o) = delete;
  percpu(percpu &&o) = delete;
  percpu &operator=(const percpu &o) = delete;

  // Ignore the safety policy and return the value of this variable
  // for the CPU that is current at some instant between entering and
  // returning from this method.
  T* get_unchecked() const {
    return cpu(myid());
  }

  T* operator->() const {
    if (S == percpu_safety::cli)
      assert(!(readrflags() & FL_IF));
    return cpu(myid());
  }

  T& operator*() const {
    if (S == percpu_safety::cli)
      assert(!(readrflags() & FL_IF));
    return *cpu(myid());
  }

  T& operator[](int id) const {
    return *cpu(id);
  }

private:
  T* cpu(int id) const {
    return &pad_[id].v_;
  }

  // percpu acts like a T* const, but since it's actually storing the
  // data directly, we have to strip the const-ness away from the data
  // being stored.  This lets const members return non-const pointers
  // to this data, just like a T* const.
  mutable struct {
    T v_ __mpalign__;
    __padout__;
  } pad_[NCPU];
};

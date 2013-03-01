#pragma once

// Critical section management

#include <stdint.h>

#include "kernel.hh"

// A critical section can be protected against several forms of
// interruption.  critical_mask controls this.
enum critical_mask {
  // Don't protect against interruption.
  NO_CRITICAL = 0,
  // Disable maskable interrupts.  Implies NO_SCHED and NO_MIGRATE.
  NO_INT = 1 << 0,
  // Disable scheduler preemption.  Implies NO_MIGRATE.
  NO_SCHED = 1 << 1,
  // Disable migration to other CPUs.
  NO_MIGRATE = 1 << 2
};

#define NO_SCHED_COUNT_YIELD_REQUESTED 0x8000000000000000

// A scoped critical section manager.
class scoped_critical
{
  critical_mask mask_;

  // Add delta to this CPU's no_sched_count.  This is atomic with
  // respect to the current CPU, but not with respect to other CPUs.
  static inline void
  modify_no_sched_count(int64_t delta)
  {
    // XXX This needs to be done in a single instruction so we don't get
    // preempted and moved to another CPU while doing this, but our
    // general per-CPU variables require two steps to compute the
    // address and then modify the variable.  If we instead had known
    // %gs offsets for general per-CPU variables, we wouldn't need
    // special support for this one.
    __asm volatile("addq %0, %%gs:(8*5)" :: "r" (delta) : "cc");
  }

  static inline uint64_t
  get_no_sched_count()
  {
    uint64_t val;
    __asm volatile("movq %%gs:(8*5), %0" : "=r" (val));
    return val;
  }

  void release_yield();

public:
  scoped_critical(critical_mask mask) : mask_(mask)
  {
    if (mask_ & NO_INT)
      pushcli();
    else if (mask_ & NO_SCHED)
      modify_no_sched_count(1);
    else if (mask_ & NO_MIGRATE)
      panic("scoped_critical(NO_MIGRATE) not implemented");
  }

  ~scoped_critical()
  {
    release();
  }

  scoped_critical(const scoped_critical &o) = delete;
  scoped_critical &operator=(const scoped_critical &o) = delete;

  scoped_critical(scoped_critical &&o) : mask_(o.mask_)
  {
    o.mask_ = NO_CRITICAL;
  }

  scoped_critical &operator=(scoped_critical &&o)
  {
    mask_ = o.mask_;
    o.mask_ = NO_CRITICAL;
    return *this;
  }

  void release()
  {
    if (mask_ & NO_INT) {
      popcli();
    } else if (mask_ & NO_SCHED) {
      modify_no_sched_count(-1);
      if (get_no_sched_count() == NO_SCHED_COUNT_YIELD_REQUESTED) {
        // We are the last one to decrease the no_sched_count when a
        // yield is requested.  It's safe to do this check because we
        // still can't be preempted in this case.  This is necessary
        // because, even though critical sections are supposed to be
        // short, if we have many back-to-back critical sections, we
        // can lose a lot of yields.
        release_yield();
      }
    }
    mask_ = NO_CRITICAL;
  }
};

// Old name for scoped_critical(NO_INT)
class scoped_cli : public scoped_critical
{
public:
  scoped_cli() : scoped_critical(NO_INT) { }
};

class scoped_no_sched : public scoped_critical
{
public:
  scoped_no_sched() : scoped_critical(NO_SCHED) { }
};

// Return true if the current context is protected against the forms
// of interruption given in mask.
bool check_critical(critical_mask mask);

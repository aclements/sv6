#pragma once

#include <atomic>
#include "bitset.hh"
#include "percpu.hh"

struct ipi_call
{
  virtual void run() = 0;

  void start(unsigned cpu);

  void wait(unsigned cpu)
  {
    while (!chain[cpu].done.load(std::memory_order_relaxed))
      nop_pause();
  }

  void run_on(const bitset<NCPU> &);

  struct chain
  {
    // next is protected by ipi_queue::lock for this chain entry's
    // CPU.
    struct ipi_call* next;
    atomic<bool> done;
    chain() : next(nullptr), done(false) { }
  };

  percpu<struct chain> chain;
};

template<class CB>
struct ipi_call_callable : public ipi_call
{
private:
  const CB &cb;

public:
  ipi_call_callable(const CB &cb) : cb(cb) { }

  void run()
  {
    cb();
  }
};

// Invoke @c cb ASAP on all CPUs in @c cpu_set.  @c cb will be called
// from the IPI interrupt handler with interrupts disabled, so it
// should be lightweight.
template<class CB>
void run_on_cpus(const bitset<NCPU> &cpu_set, CB cb)
{
  ipi_call_callable<CB> call(cb);
  call.run_on(cpu_set);
}

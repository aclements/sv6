#pragma once

#include <atomic>
#include "bitset.hh"
#include "percpu.hh"

struct ipi_call
{
  void run_on(const bitset<NCPU> &);

protected:
  virtual void run() = 0;

private:
  void start(unsigned cpu);

  // next[i] must be valid before this call is linked in to the
  // myipi[i] queue and must not change until the call is done.
  struct ipi_call* next[NCPU];
  __mpalign__ atomic<int> waiting;
  __padout__;

  friend void on_ipicall();
};

template<class CB>
struct ipi_call_callable : public ipi_call
{
private:
  const CB &cb;

public:
  ipi_call_callable(const CB &cb) : cb(cb) { }

protected:
  void run() override
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

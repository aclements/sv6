#include "ipi.hh"

#include "apic.hh"
#include "traps.h"

#include <iterator>

using namespace std;

void
ipi_call::start(unsigned cpu)
{
  bool need_ipi = false;
  auto &c = cpus[cpu];
  {
    auto l = c.ipi_lock.guard();
    if (!c.ipi.load(memory_order_relaxed))
      need_ipi = true;
    (*c.ipi_tail).store(this, memory_order_relaxed);
    c.ipi_tail = &this->chain[cpu].next;
  }
  if (need_ipi)
    lapic->send_ipi(&c, T_IPICALL);
}

void
ipi_call::run_on(const bitset<NCPU> &cpus)
{
  // If we're called with interrupts enabled, then we might be
  // migrated during this, but it's also safe to self-IPI.  If we're
  // called with interrupts disabled, then we can't self-IPI, but it's
  // safe to do a local call.
  bool interruptable = readrflags() & FL_IF;
  unsigned id = interruptable ? -1 : myid();
  for (auto cpu : cpus)
    if (cpu != id)
      start(cpu);
  if (!interruptable && cpus[id])
    run();
  for (auto cpu : cpus)
    if (cpu != id)
      wait(cpu);
}

// Handle an IPI call interrupt on this CPU.
void
on_ipicall()
{
  assert(!(readrflags() & FL_IF));
  auto cpu = mycpu();
  ipi_call *call = cpu->ipi.load(memory_order_relaxed);
  while (call) {
    call->run();

    auto next = call->chain[cpu->id].next.load(memory_order_relaxed);
    if (!next) {
      // Check under protection of the list lock
      auto l = cpu->ipi_lock.guard();
      next = call->chain[cpu->id].next.load(memory_order_relaxed);
      if (!next) {
        // We've consumed the list.  Reset it, which will also
        // indicate to other CPUs that they should again send an
        // interrupt if they add to the call list.
        cpu->ipi.store(nullptr, memory_order_relaxed);
        cpu->ipi_tail = &cpu->ipi;
      }
    }

    // This call is done.  After we mark it done, we can't use any
    // fields because it may be freed by its creator.
    call->chain[cpu->id].done.store(true, memory_order_relaxed);

    call = next;
  }
}

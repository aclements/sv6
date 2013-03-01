#include "ipi.hh"

#include "apic.hh"
#include "traps.h"
#include "bits.hh"

#include <iterator>

using namespace std;

struct ipi_queue
{
  spinlock lock;
  struct ipi_call *head;
  struct ipi_call **tail;
  bool ipicall_active;

  ipi_queue()
    : lock("ipi_queue::lock"), head(nullptr), tail(&head),
      ipicall_active(false) { }
};

DEFINE_PERCPU(struct ipi_queue, myipi);

void
ipi_call::start(unsigned cpu)
{
  bool need_ipi = false;
  auto &q = myipi[cpu];
  {
    auto l = q.lock.guard();
    if (!(q.head || q.ipicall_active))
      need_ipi = true;
    next[cpu] = nullptr;
    *q.tail = this;
    q.tail = &this->next[cpu];
  }
  if (need_ipi)
    lapic->send_ipi(&cpus[cpu], T_IPICALL);
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
  waiting = cpus.count() - (!interruptable && cpus[id]);
  for (auto cpu : cpus)
    if (cpu != id)
      start(cpu);
  if (!interruptable && cpus[id])
    run();
  while (waiting.load(memory_order_relaxed))
    nop_pause();
}

// Handle an IPI call interrupt on this CPU.
void
on_ipicall()
{
  assert(!(readrflags() & FL_IF));
  auto id = myid();
  while (true) {
    // Get the IPI call list
    auto &q = *myipi;
    auto l = q.lock.guard();
    ipi_call *call = q.head;
    q.head = nullptr;
    q.tail = &q.head;
    q.ipicall_active = !!call;
    if (!call)
      break;
    l.release();

    // Walk the call list
    while (call) {
      call->run();

      ipi_call *next = call->next[id];

      // This call is done.  After we mark it done, we can't use any
      // fields because it may be freed immediately by its creator.
      call->waiting.fetch_sub(1, memory_order_relaxed);

      call = next;
    }
  }
}

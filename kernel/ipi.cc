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

/* Logic for implementing pause_other_cpus_and_call */
std::atomic<int> paused_cpu_counter {0};
enum pause_state_t { Ready, Pausing, Paused, Unpausing, Unpaused };
std::atomic<pause_state_t> pause_state {Ready};

// Use IPI to pause other cores until update is complete.
void
pause_other_cpus(void)
{
  // wait for any previously paused CPUs to resume
  pause_state_t READY = Ready;
  while (!atomic_compare_exchange_strong(&pause_state, &READY, Pausing)) nop_pause();

  pushcli();
  for (cpuid_t i = 0; i < ncpu; ++i) {
    if (i != myid())
      lapic->send_pause(&cpus[i]);
  }

  // wait for other CPUs to actually pause
  while (pause_state != Paused) nop_pause();
}

void
resume_other_cpus(void)
{
  pause_state = Unpausing;
  while (pause_state != Unpaused) nop_pause();
  popcli();
  pause_state = Ready;
}

void
pause_other_cpus_and_call(void (*fn)(void))
{
  if (fn != NULL) {
    pause_other_cpus();
    fn();
    resume_other_cpus();
  }
}

void
pause_cpu(void)
{
  if (DEBUG || true)
    cprintf("pausing cpu %d\n", mycpu()->id);
  paused_cpu_counter++;
  if (paused_cpu_counter == ncpu - 1)
    pause_state = Paused;

  // spin until the core that called pause_other_cpus_and_call is done
  while (pause_state != Unpausing) nop_pause();

  if (DEBUG || true)
    cprintf("resuming cpu %d\n", mycpu()->id);
  paused_cpu_counter--;
  if (paused_cpu_counter == 0)
    pause_state = Unpaused;
}

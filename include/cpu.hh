#pragma once

#include "mmu.h"
#include <atomic>
#include "spinlock.h"
#include "spercpu.hh"

using std::atomic;
namespace MMU_SCHEME {
  class page_map_cache;
};

// Per-CPU state
struct cpu {
  // XXX(Austin) We should move more of this out to static_percpu's.
  // The only things that need to live here are the fast-access
  // %gs-relative fields at the end (with a little more
  // sophistication, we could probably get rid of those, too).

  cpuid_t id;                  // Index into cpus[] below
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct context *scheduler;   // swtch() here to enter scheduler

  int timer_printpc;
  __mpalign__
  atomic<u64> tlbflush_done;   // last tlb flush req done on this cpu
  atomic<u64> tlb_cr3;         // current value of cr3 on this cpu
  __padout__;
  struct proc *prev;           // The previously-running process
  atomic<struct proc*> fpu_owner; // The proc with the current FPU state
  struct numa_node *node;

  hwid_t hwid __mpalign__;     // Local APIC ID, accessed by other CPUs
  __padout__;

  // Cpu-local storage variables; see below and in spercpu.hh
  struct cpu *cpu;
  struct proc *proc;           // The currently-running process.
  struct cpu_mem *mem;         // The per-core memory metadata
  u64 syscallno;               // Temporary used by sysentry
  void *percpu_base;           // Per-CPU memory region base
  uint64_t no_sched_count;     // sched disable count; high bit means
                               // yield requested
} __mpalign__;

DECLARE_PERCPU(struct cpu, cpus);

// Per-CPU variables, holding pointers to the
// current cpu and to the current process.
// XXX(sbw) asm labels default to RIP-relative and
// I don't know how to force absolute addressing.
static inline struct cpu *
mycpu(void)
{
  u64 val;
  __asm volatile("movq %%gs:0, %0" : "=r" (val));
  return (struct cpu *)val;
}

static inline struct proc *
myproc(void)
{
  u64 val;
  __asm volatile("movq %%gs:8, %0" : "=r" (val));
  return (struct proc *)val;
}

static inline cpuid_t
myid(void)
{
  return mycpu()->id;
}

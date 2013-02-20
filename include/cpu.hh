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
  MMU_SCHEME::page_map_cache *curcache;

  // The list of IPI calls to this CPU
  __mpalign__
  atomic<struct ipi_call *> ipi __mpalign__;
  atomic<struct ipi_call *> *ipi_tail;
  // The lock protecting updates to ipi and ipi_tail.
  spinlock ipi_lock;
  __padout__;

  hwid_t hwid __mpalign__;     // Local APIC ID, accessed by other CPUs
  __padout__;

  // Cpu-local storage variables; see below and in spercpu.hh
  struct cpu *cpu;
  struct proc *proc;           // The currently-running process.
  struct cpu_mem *mem;         // The per-core memory metadata
  u64 syscallno;               // Temporary used by sysentry
  void *percpu_base;           // Per-CPU memory region base
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

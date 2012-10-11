#pragma once

#include "mmu.h"
#include "atomic.hh"
#ifndef XV6_USER
// XXX(Austin) lib/wqalloc.c includes percpu, which includes cpu.hh.
// I don't understand at all how percpu works in user space, but
// spinlock definitely doesn't work in user space.
#include "spinlock.h"
#endif

using std::atomic;

// Per-CPU state
struct cpu {
  cpuid_t id;                  // Index into cpus[] below
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct context *scheduler;   // swtch() here to enter scheduler

  int timer_printpc;
  atomic<u64> tlbflush_done;   // last tlb flush req done on this cpu
  struct proc *prev;           // The previously-running process

#ifndef XV6_USER
  // The list of IPI calls to this CPU
  atomic<struct ipi_call *> ipi __mpalign__;
  atomic<struct ipi_call *> *ipi_tail;
  // The lock protecting updates to ipi and ipi_tail.
  spinlock ipi_lock;
#endif

  hwid_t hwid __mpalign__;     // Local APIC ID, accessed by other CPUs

  // Cpu-local storage variables; see below
  struct cpu *cpu;
  struct proc *proc;           // The currently-running process.
  struct cpu_mem *mem;         // The per-core memory metadata
  u64 syscallno;               // Temporary used by sysentry
} __mpalign__;

extern struct cpu cpus[NCPU];

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

static inline struct kmem *
mykmem(void)
{
  u64 val;
  __asm volatile("movq %%gs:16, %0" : "=r" (val));
  return (struct kmem *)val;
}

static inline cpuid_t
myid(void)
{
  return mycpu()->id;
}

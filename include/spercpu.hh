// Per-CPU variables with static duration

// We isolate percpu variables in a special section called .percpu,
// which kernel.ld places just after the bss.  The .percpu section in
// the kernel image itself becomes the per-CPU storage for the boot
// CPU.  For other CPUs, we allocate a separate region in that CPU's
// NUMA node of the same size as .percpu.  Then, to find a CPU's
// instance of a per-CPU variable, we simply offset that variable's
// address from the .percpu section into the other CPU's region.  This
// approach lets the linker do most of the heavy lifting for us, packs
// per-CPU data nice (unlike creating arrays for each variable), and
// makes it easy to associate per-CPU variables with the appropriate
// NUMA nodes.

#pragma once

#include "critical.hh"

#include <cstddef>
#include <new>

// This defines CPU 0's instance of this percpu variable and the
// static_percpu wrapper for accessing it.  The gunk after the section
// makes .percpu a BSS-like section (allocated, writable, but with no
// data in the object).  It also create an initializer function and
// records a pointer to it in the .percpuinit_array section.
#define DEFINE_PERCPU(type, name, ...)                                  \
  DEFINE_PERCPU_NOINIT(type, name, ##__VA_ARGS__);                      \
  static void __##name##_init(size_t c) { new(&name[c]) type(); }       \
  static void (*__##name##_initp)(size_t)                               \
    __attribute__((section(".percpuinit_array"),used)) = __##name##_init;

// This is like DEFINE_PERCPU, but doesn't call the class's
// constructor.  This is for special cases like cpus that must be
// initialized remotely.
#define DEFINE_PERCPU_NOINIT(type, name, ...)                           \
  type __##name##_key __attribute__((__section__(".percpu,\"aw\",@nobits#"))); \
  static_percpu<type, &__##name##_key, ##__VA_ARGS__> name

#define DECLARE_PERCPU(type, name, ...) \
  extern type __##name##_key;                \
  extern static_percpu<type, &__##name##_key, ##__VA_ARGS__> name

// The base of each CPU's per-CPU region.  This is used to find other
// CPU's regions.  This pointer is also stored in struct cpu; that
// copy is used to quickly find our own CPU's region.
extern void *percpu_offsets[NCPU];

// The base of core 0's percpu variable block.  Provided by the
// linker.
extern char __percpu_start[];

// A per-CPU variable of type T at location 'key' in CPU 0's per-CPU
// region.  For debugging, CM specifies how interruptable the current
// context can be when accessing this per-CPU variable.  It defaults
// to requiring the preemption be disabled to prevent the accessing
// thread from migrating, but for per-CPU variables that may be
// accessed by interrupt handlers, it should be set to NO_INT.
template<class T, T *key, critical_mask CM = NO_SCHED>
struct static_percpu
{
  constexpr static_percpu() = default;

  static_percpu(const static_percpu &o) = delete;
  static_percpu(static_percpu &&o) = delete;
  static_percpu &operator=(const static_percpu &o) = delete;
  static_percpu &operator=(static_percpu &&o) = delete;

  T* get_unchecked() const
  {
    uintptr_t val;
    // The per-CPU memory offset is stored at %gs:32.
    // XXX Having to subtract __percpu_start makes this several
    // instructions longer than strictly necessary.  Alternatively, we
    // could locate .percpu at address 0 and use the key as a direct
    // offset.
    __asm("add %%gs:32, %0" : "=r" (val) : "0" ((char*)key - __percpu_start));
    return (T*)val;
  }

  T* get() const
  {
#if DEBUG
    assert(check_critical(CM));
#endif
    return get_unchecked();
  }

  T* operator->() const
  {
    return get();
  }

  T& operator*() const
  {
    return *get();
  }

  T& operator[](int id) const
  {
    assert(percpu_offsets[id]);
    return *(T*)((char*)percpu_offsets[id] + ((char*)key - __percpu_start));
  }
};

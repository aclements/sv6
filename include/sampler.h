#include <stdint.h>
#include <stdbool.h>

// An event selector
struct perf_selector
{
  bool enable : 1;
  // Enable precise sampling, if possible.  This is incompatible with
  // recording stack traces.
  bool precise : 1;
  // The event selector code.  This is architecture-specific, but
  // generally includes an event number, a unit mask, and various
  // other flags.  Any interrupt flag is ignored.
  uint64_t selector;
  // If non-zero, record the current instruction pointer every
  // 'period' events.
  uint64_t period;
};

#define NTRACE 4

struct pmuevent {
  u8 idle:1;
  u32 count;
  u64 rip;
  uptr trace[NTRACE];
};

struct logheader {
  u32 ncpus;
  struct {
    u64 offset;
    u64 size;
  } cpu[];
} __attribute__((packed));

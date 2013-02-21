#include <stdint.h>
#include <stdbool.h>

// An event selector
struct perf_selector
{
  bool enable : 1;
  // Enable precise sampling, if possible.  This is incompatible with
  // recording stack traces.
  bool precise : 1;
  // If non-zero, profile load latency by sampling loads that take
  // load_latency or more CPU cycles.  This is only supported on Intel
  // Nehalem or later.  If this is set, the value must be at least 4,
  // precise must be set, and selector must be
  // MEM_INST_RETIRED.LATENCY_ABOVE_THRESHOLD (0x100B), with cmask and
  // inv set to 0.  See Intel SDM Volume 3 for details on exactly what
  // this measures.
  // XXX Sandy Bridge changed the required selector.
  uint16_t load_latency;
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
  u8 ints_disabled:1;
  u8 kernel:1;
  u32 count;
  u64 rip;
  uptr trace[NTRACE];
  u32 latency, data_source;
  u64 load_address;
};

struct logheader {
  u64 ncpus;
  struct {
    u64 offset;
    u64 size;
  } cpu[];
} __attribute__((packed));
